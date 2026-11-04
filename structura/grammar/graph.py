"""DesignGraph: an ordered list of typed grammar features over named, bounded design parameters.

Validation runs before any geometry is built: unknown feature types or parameters, missing values, expressions
that refer to undeclared parameters, values outside physical bounds and references to features that do not exist
earlier in the graph are all rejected with a message that names the feature and parameter.
"""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Literal

from pydantic import BaseModel, Field, model_validator

from .expr import ExprError, evaluate, names_in
from .features import AXES, FEATURES, KINDS


class GraphError(ValueError):
    pass


class Param(BaseModel):
    unit: str = "mm"
    low: float
    high: float
    default: float
    integer: bool = False
    doc: str = ""

    @model_validator(mode="after")
    def _check(self):
        if not self.low <= self.default <= self.high:
            raise ValueError(f"default {self.default} outside [{self.low}, {self.high}]")
        return self


class Feature(BaseModel):
    id: str = Field(pattern=r"^[a-z][a-z0-9_]*$")
    type: str
    params: dict[str, Any] = {}
    doc: str = ""


class FaceSelector(BaseModel):
    """Selects faces of the final part by geometry, so names survive parameter changes."""
    kind: Literal["plane", "cylinder"]
    normal: str | None = None            # plane: axis name the face normal points along (sign matters)
    at: Any = None                       # plane: coordinate of the face along its normal axis (expression)
    axis: str | None = None              # cylinder: axis direction (sign ignored)
    radius: Any = None                   # cylinder: radius (expression)
    region: list[Any] | None = None      # optional [xmin, ymin, zmin, xmax, ymax, zmax] the face centre must lie in
    doc: str = ""


class DesignGraph(BaseModel):
    name: str
    version: str = "1"
    doc: str = ""
    parameters: dict[str, Param] = {}
    features: list[Feature]
    faces: dict[str, FaceSelector] = {}
    design_space: list[Any] | None = None

    # ------------------------------------------------------------------ io
    @classmethod
    def load(cls, path: str | Path) -> "DesignGraph":
        return cls.model_validate(json.loads(Path(path).read_text(encoding="utf-8")))

    def canonical_json(self) -> str:
        return json.dumps(self.model_dump(mode="json"), sort_keys=True, separators=(",", ":"))

    # ------------------------------------------------------------------ values
    def values(self, overrides: dict[str, float] | None = None) -> dict[str, float]:
        """Default parameter values with overrides applied and checked against bounds."""
        vals = {k: p.default for k, p in self.parameters.items()}
        for k, v in (overrides or {}).items():
            if k not in self.parameters:
                raise GraphError(f"unknown parameter {k!r}")
            vals[k] = float(v)
        for k, v in vals.items():
            p = self.parameters[k]
            if not p.low - 1e-9 <= v <= p.high + 1e-9:
                raise GraphError(f"parameter {k!r} = {v} outside [{p.low}, {p.high}] {p.unit}")
            if p.integer and abs(v - round(v)) > 1e-9:
                raise GraphError(f"parameter {k!r} must be an integer, got {v}")
            if p.integer:
                vals[k] = float(round(v))
        return vals

    # ------------------------------------------------------------------ validation
    def resolve(self, overrides: dict[str, float] | None = None) -> list[tuple[Feature, dict[str, Any]]]:
        """Validate the graph for the given parameter values and return each feature with evaluated values."""
        vals = self.values(overrides)
        seen: dict[str, str] = {}
        out = []
        if not self.features:
            raise GraphError("graph has no features")
        for i, f in enumerate(self.features):
            where = f"feature {f.id!r} ({f.type})"
            if f.id in seen:
                raise GraphError(f"{where}: duplicate id")
            ft = FEATURES.get(f.type)
            if ft is None:
                raise GraphError(f"{where}: unknown feature type; known types: {', '.join(sorted(FEATURES))}")
            unknown = set(f.params) - set(ft.params)
            if unknown:
                raise GraphError(f"{where}: unknown parameters {sorted(unknown)}")
            res: dict[str, Any] = {}
            for name, spec in ft.params.items():
                raw = f.params.get(name, spec.default)
                if raw is None:
                    raise GraphError(f"{where}: missing parameter {name!r}")
                try:
                    res[name] = self._value(spec.kind, raw, vals, seen, f"{where}.{name}")
                except ExprError as e:
                    raise GraphError(f"{where}.{name}: {e}") from None
            if i == 0 and (ft.modifies or ft.pattern or res.get("op", "add") == "cut" or f.type in ("hole", "hole_pattern", "pocket")):
                raise GraphError(f"{where}: the first feature must add material")
            seen[f.id] = f.type
            out.append((f, res))
        for name, sel in self.faces.items():
            self._check_selector(name, sel, vals)
        return out

    def _value(self, kind, raw, vals, seen, where):
        if kind in KINDS:
            v = evaluate(raw, vals)
            lo, hi, unit = KINDS[kind]
            if not lo <= v <= hi:
                raise GraphError(f"{where} = {v:g} outside physical bounds [{lo}, {hi}] {unit}")
            if kind == "count":
                if abs(v - round(v)) > 1e-9:
                    raise GraphError(f"{where} must be a whole number, got {v:g}")
                v = int(round(v))
            return v
        if kind == "axis":
            if raw not in AXES:
                raise GraphError(f"{where} = {raw!r} is not one of {sorted(AXES)}")
            return raw
        if kind == "op":
            if raw not in ("add", "cut"):
                raise GraphError(f"{where} must be 'add' or 'cut'")
            return raw
        if kind == "ref":
            if raw not in seen:
                raise GraphError(f"{where} refers to {raw!r}, which is not an earlier feature")
            if FEATURES[seen[raw]].modifies or FEATURES[seen[raw]].pattern:
                raise GraphError(f"{where} refers to {raw!r}, which has no primitives to repeat")
            return raw
        if kind in ("points", "vec", "bbox"):
            if not isinstance(raw, list):
                raise GraphError(f"{where} must be a list")
            if kind == "points":
                if len(raw) < 2:
                    raise GraphError(f"{where} needs at least two points")
                return [[self._value("pos", c, vals, seen, where) for c in p] for p in raw]
            n = 3 if kind == "vec" else 6
            if len(raw) != n:
                raise GraphError(f"{where} needs {n} values")
            return [self._value("pos", c, vals, seen, where) for c in raw]
        raise GraphError(f"{where}: unknown parameter kind {kind}")

    def _check_selector(self, name, sel, vals):
        where = f"face {name!r}"
        try:
            if sel.kind == "plane":
                if sel.normal not in AXES or sel.at is None:
                    raise GraphError(f"{where}: a plane selector needs normal and at")
                evaluate(sel.at, vals)
            else:
                if sel.axis not in AXES or sel.radius is None:
                    raise GraphError(f"{where}: a cylinder selector needs axis and radius")
                evaluate(sel.radius, vals)
            if sel.region is not None:
                if len(sel.region) != 6:
                    raise GraphError(f"{where}: region needs 6 values")
                [evaluate(c, vals) for c in sel.region]
        except ExprError as e:
            raise GraphError(f"{where}: {e}") from None

    def referenced_names(self) -> set[str]:
        """Every parameter name used anywhere in the graph (for checking free parameters are wired in)."""
        out = set()

        def walk(x):
            if isinstance(x, str):
                try:
                    out.update(names_in(x))
                except ExprError:
                    pass
            elif isinstance(x, list):
                for y in x:
                    walk(y)
            elif isinstance(x, dict):
                for y in x.values():
                    walk(y)
        walk([f.params for f in self.features])
        walk([s.model_dump() for s in self.faces.values()])
        walk(self.design_space or [])
        return out
