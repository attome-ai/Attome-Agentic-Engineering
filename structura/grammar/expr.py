"""Safe arithmetic expressions over named design parameters.

A feature value is a number or a string expression such as "thickness * 2 + 5" or "max(3, w / 4)".
Only arithmetic, comparisons, the functions below and declared parameter names are allowed; anything else
(attributes, calls to other names, subscripts, lambdas) is rejected before evaluation.
"""
from __future__ import annotations

import ast
import math
import operator

_BIN = {ast.Add: operator.add, ast.Sub: operator.sub, ast.Mult: operator.mul, ast.Div: operator.truediv,
        ast.FloorDiv: operator.floordiv, ast.Mod: operator.mod, ast.Pow: operator.pow}
_UN = {ast.USub: operator.neg, ast.UAdd: operator.pos}
_FUN = {"min": min, "max": max, "abs": abs, "round": round, "int": int, "sqrt": math.sqrt,
        "sin": lambda d: math.sin(math.radians(d)), "cos": lambda d: math.cos(math.radians(d)),
        "tan": lambda d: math.tan(math.radians(d))}


class ExprError(ValueError):
    pass


def names_in(expr: str) -> set[str]:
    """Parameter names an expression refers to (function names excluded)."""
    tree = _parse(expr)
    out = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Name) and node.id not in _FUN:
            out.add(node.id)
    return out


def _parse(expr: str) -> ast.Expression:
    try:
        tree = ast.parse(expr, mode="eval")
    except SyntaxError as e:
        raise ExprError(f"cannot parse expression {expr!r}: {e.msg}") from None
    for node in ast.walk(tree):
        ok = isinstance(node, (ast.Expression, ast.BinOp, ast.UnaryOp, ast.Constant, ast.Name, ast.Load, ast.Call,
                               ast.operator, ast.unaryop))
        if not ok:
            raise ExprError(f"expression {expr!r} uses a forbidden construct: {type(node).__name__}")
        if isinstance(node, ast.Call) and not (isinstance(node.func, ast.Name) and node.func.id in _FUN):
            raise ExprError(f"expression {expr!r} calls an unknown function")
        if isinstance(node, ast.Constant) and not isinstance(node.value, (int, float)):
            raise ExprError(f"expression {expr!r} contains a non-numeric constant")
    return tree


def evaluate(value, params: dict[str, float]) -> float:
    """Evaluate a number or expression string with the given parameter values."""
    if isinstance(value, bool):
        raise ExprError("booleans are not numeric values")
    if isinstance(value, (int, float)):
        return float(value)
    if not isinstance(value, str):
        raise ExprError(f"value {value!r} is neither a number nor an expression")
    tree = _parse(value)

    def ev(n):
        if isinstance(n, ast.Expression):
            return ev(n.body)
        if isinstance(n, ast.Constant):
            return n.value
        if isinstance(n, ast.Name):
            if n.id not in params:
                raise ExprError(f"expression {value!r} refers to unknown parameter {n.id!r}")
            return params[n.id]
        if isinstance(n, ast.BinOp):
            return _BIN[type(n.op)](ev(n.left), ev(n.right))
        if isinstance(n, ast.UnaryOp):
            return _UN[type(n.op)](ev(n.operand))
        if isinstance(n, ast.Call):
            return _FUN[n.func.id](*[ev(a) for a in n.args])
        raise ExprError(f"unsupported node {type(n).__name__}")

    try:
        out = ev(tree)
    except ZeroDivisionError:
        raise ExprError(f"expression {value!r} divides by zero") from None
    return float(out)
