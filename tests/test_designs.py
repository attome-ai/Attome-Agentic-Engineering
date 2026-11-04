"""B3-T3: designs round-trip through disk with stable hashes."""
import subprocess
import sys

from structura.designs import Design
from structura.templates.bracket import BRACKET


def test_equal_inputs_equal_hash():
    a = Design(BRACKET.graph, {"thickness": 5.0})
    b = Design(BRACKET.graph, {"thickness": 5.0000000001})
    assert a.hash == b.hash


def test_any_change_new_hash():
    a = Design(BRACKET.graph, {"thickness": 5.0})
    assert a.hash != Design(BRACKET.graph, {"thickness": 5.1}).hash
    g2 = BRACKET.graph.model_copy(deep=True)
    g2.version = "2"
    assert a.hash != Design(g2, {"thickness": 5.0}).hash


def test_round_trip(tmp_path):
    a = Design(BRACKET.graph, {"thickness": 7, "rib_count": 2})
    a.save(tmp_path / "d")
    b = Design.load(tmp_path / "d")
    assert b.hash == a.hash and b.params == a.params


def test_saved_script_rebuilds(tmp_path):
    a = Design(BRACKET.graph, {"thickness": 7})
    a.save(tmp_path / "d")
    code = (tmp_path / "d" / "design.py").read_text() + "\nprint(round(shape.Volume(), 3))\n"
    out = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, timeout=120)
    assert out.returncode == 0, out.stderr
    assert abs(float(out.stdout.strip()) - a.build().shape.Volume()) < 1e-3
