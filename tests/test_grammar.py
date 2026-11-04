"""B3-T1: every grammar feature compiles at the low and high corner of its bounds; invalid graphs fail before build."""
import itertools

import pytest

from structura.grammar.compile import compile_graph
from structura.grammar.features import FEATURES
from structura.grammar.graph import DesignGraph, GraphError

BASE = {"id": "base", "type": "plate", "params": {"x": 0, "y": 0, "z": 0, "length": 100, "width": 100, "thickness": 20}}

# One fixture per feature type: free parameters with bounds, and the feature(s) using them on a 100 x 100 x 20 plate.
FIXTURES = {
    "plate": ({"t": (2, 30)}, [{"id": "f", "type": "plate", "params": {"x": 0, "y": 0, "z": 20, "length": 40, "width": 40, "thickness": "t"}}]),
    "extrude_sketch": ({"h": (1, 40)}, [{"id": "f", "type": "extrude_sketch", "params": {
        "points": [[10, 10], [60, 10], [35, 50]], "z": 20, "height": "h"}}]),
    "boss": ({"d": (4, 40), "h": (1, 30)}, [{"id": "f", "type": "boss", "params": {"x": 50, "y": 50, "z": 20, "diameter": "d", "height": "h"}}]),
    "hole": ({"d": (1, 30)}, [{"id": "f", "type": "hole", "params": {"x": 50, "y": 50, "z": 20, "diameter": "d", "depth": 20}}]),
    "hole_pattern": ({"n": (1, 4), "d": (2, 10)}, [{"id": "f", "type": "hole_pattern", "params": {
        "x": 15, "y": 15, "z": 20, "diameter": "d", "depth": 20, "n1": "n", "d1": 20, "n2": 2, "d2": 30}}]),
    "rib": ({"t": (1, 20), "a": (5, 60)}, [
        {"id": "wall", "type": "plate", "params": {"x": 0, "y": 0, "z": 20, "length": 10, "width": 100, "thickness": 60}},
        {"id": "f", "type": "rib", "params": {"x": 10, "y": 40, "z": 20, "leg_a": "a", "leg_a_dir": "x", "leg_b": "a",
                                              "leg_b_dir": "z", "thickness": "t"}}]),
    "pocket": ({"r": (0, 9), "dep": (1, 15)}, [{"id": "f", "type": "pocket", "params": {
        "x": 20, "y": 20, "z_top": 20, "length": 40, "width": 20, "depth": "dep", "corner_radius": "r"}}]),
    "fillet": ({"r": (0.5, 8)}, [{"id": "f", "type": "fillet", "params": {
        "radius": "r", "edge_axis": "y", "region": [-1, -1, 19, 101, 101, 21]}}]),
    "chamfer": ({"c": (0.5, 8)}, [{"id": "f", "type": "chamfer", "params": {
        "length": "c", "edge_axis": "x", "region": [-1, -1, 19, 101, 101, 21]}}]),
    "shell": ({"t": (1, 8)}, [{"id": "f", "type": "shell", "params": {"thickness": "t", "open_side": "z"}}]),
    "linear_pattern": ({"n": (1, 5)}, [
        {"id": "h", "type": "hole", "params": {"x": 10, "y": 50, "z": 20, "diameter": 6, "depth": 20}},
        {"id": "f", "type": "linear_pattern", "params": {"target": "h", "count": "n", "dx": 18}}]),
    "circular_pattern": ({"n": (1, 8)}, [
        {"id": "h", "type": "hole", "params": {"x": 80, "y": 50, "z": 20, "diameter": 6, "depth": 20}},
        {"id": "f", "type": "circular_pattern", "params": {"target": "h", "count": "n", "cx": 50, "cy": 50}}]),
    "sweep_path": ({"r": (1, 6)}, [{"id": "f", "type": "sweep_path", "params": {
        "points": [[10, 10, "20 + r * 0.5"], [90, 10, "20 + r * 0.5"], [90, 90, "20 + r * 0.5"]], "radius": "r"}}]),
    "fin_array": ({"n": (1, 10), "h": (2, 50)}, [{"id": "f", "type": "fin_array", "params": {
        "x": 0, "y": 2, "z": 20, "count": "n", "pitch": 10, "length": 100, "fin_thickness": 2, "height": "h"}}]),
}


def test_every_feature_has_a_fixture():
    assert set(FIXTURES) == set(FEATURES)


def graph_for(ftype):
    params, feats = FIXTURES[ftype]
    return DesignGraph.model_validate({
        "name": f"fixture_{ftype}",
        "parameters": {k: {"low": lo, "high": hi, "default": lo, "integer": isinstance(lo, int) and isinstance(hi, int) and k == "n"}
                       for k, (lo, hi) in params.items()},
        "features": [BASE] + feats})


@pytest.mark.parametrize("ftype", sorted(FIXTURES))
def test_feature_compiles_at_bound_corners(ftype):
    g = graph_for(ftype)
    names = list(g.parameters)
    for corner in itertools.product(*[(g.parameters[k].low, g.parameters[k].high) for k in names]):
        part = compile_graph(g, dict(zip(names, corner)))
        solids = part.solids()
        assert len(solids) == 1, f"{ftype} at {corner}: {len(solids)} solids"
        assert solids[0].isValid(), f"{ftype} at {corner}: invalid solid"
        assert solids[0].Volume() > 0


def test_out_of_bounds_value_rejected_before_build():
    g = graph_for("boss")
    with pytest.raises(GraphError, match="'d'"):
        g.resolve({"d": 100})


def test_missing_parent_rejected():
    g = DesignGraph.model_validate({"name": "bad", "features": [BASE, {"id": "p", "type": "linear_pattern",
                                    "params": {"target": "nope", "count": 2, "dx": 5}}]})
    with pytest.raises(GraphError, match="not an earlier feature"):
        g.resolve()


def test_unknown_parameter_in_expression_rejected():
    g = DesignGraph.model_validate({"name": "bad", "features": [
        {"id": "base", "type": "plate", "params": {"x": 0, "y": 0, "z": 0, "length": "L", "width": 10, "thickness": 2}}]})
    with pytest.raises(GraphError, match="unknown parameter 'L'"):
        g.resolve()


def test_code_injection_rejected():
    g = DesignGraph.model_validate({"name": "bad", "features": [
        {"id": "base", "type": "plate", "params": {"x": 0, "y": 0, "z": 0, "length": "__import__('os')", "width": 10, "thickness": 2}}]})
    with pytest.raises(GraphError):
        g.resolve()


def test_first_feature_must_add():
    g = DesignGraph.model_validate({"name": "bad", "features": [
        {"id": "h", "type": "hole", "params": {"x": 0, "y": 0, "z": 0, "diameter": 5, "depth": 5}}]})
    with pytest.raises(GraphError, match="must add material"):
        g.resolve()
