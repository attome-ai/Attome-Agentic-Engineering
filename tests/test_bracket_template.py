"""B3-T2: the bracket family builds from its graph for default and corner parameters, with named faces."""
import itertools

import pytest

from structura.grammar.graph import GraphError
from structura.templates.bracket import BRACKET


def check(part):
    solids = part.solids()
    assert len(solids) == 1, f"{len(solids)} solids for {part.values}"
    assert solids[0].isValid()
    faces = BRACKET.named_faces(part)
    assert len(faces["mount_holes"]) >= 2, faces["mount_holes"]
    for name in ("load_face", "top_face", "wall_face"):
        assert len(faces[name]) >= 1, f"{name} empty for {part.values}"
    return solids[0]


def test_default_build():
    s = check(BRACKET.build())
    # wall 6*50*100 + arm 120*50*6 - overlap 6*50*6 + rib - holes: sanity range in mm^3
    assert 60_000 < s.Volume() < 80_000


def test_corner_builds():
    names = list(BRACKET.free)
    for corner in itertools.product(*[(BRACKET.free[k].low, BRACKET.free[k].high) for k in names]):
        check(BRACKET.build(dict(zip(names, corner))))


def test_out_of_bounds_names_parameter():
    with pytest.raises(GraphError, match="thickness"):
        BRACKET.build({"thickness": 40})


def test_non_integer_rib_count_rejected():
    with pytest.raises(GraphError, match="rib_count"):
        BRACKET.build({"rib_count": 1.5})
