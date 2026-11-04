"""F2-T1 smoke test: the C++ engine builds a part and meshes it with gmsh, and ccx solves a one-element cube."""
from structura import engine
from structura.tools import find_exe, run


def test_engine_build_and_mesh(tmp_path):
    b = engine.build("bracket", {"thickness": 6}, out=tmp_path / "build")
    assert b["solids"] == 1 and b["valid"]
    assert (tmp_path / "build" / "part.step").stat().st_size > 0
    m = engine.mesh("bracket", {"thickness": 6}, tmp_path / "mesh", size=6.0)
    assert m["elements"] > 500 and all(g["ok"] for g in m["groups"].values())


def test_engine_errors_are_typed():
    try:
        engine.build("bracket", {"thickness": 40})
    except engine.EngineError as e:
        assert e.kind == "graph" and "thickness" in str(e)
    else:
        raise AssertionError("expected an EngineError")


def test_ccx_solves_cube(tmp_path):
    # 1 x 1 x 1 mm C3D8 block, E = 210000 MPa, 210 N tension -> strain 1e-3 -> 0.001 mm elongation.
    # ccx -v exits 201 by design, so a real solve is the check; CalculiX 2.23 needs NALL defined explicitly.
    deck = """*NODE
1,0,0,0
2,1,0,0
3,1,1,0
4,0,1,0
5,0,0,1
6,1,0,1
7,1,1,1
8,0,1,1
*ELEMENT,TYPE=C3D8,ELSET=EALL
1,1,2,3,4,5,6,7,8
*NSET,NSET=NALL,GENERATE
1,8,1
*BOUNDARY
1,1,3
4,1,1
4,3,3
5,1,2
8,1,1
*MATERIAL,NAME=STEEL
*ELASTIC
210000,0.0
*SOLID SECTION,ELSET=EALL,MATERIAL=STEEL
*STEP
*STATIC
*CLOAD
2,1,52.5
3,1,52.5
6,1,52.5
7,1,52.5
*NODE PRINT,NSET=NALL
U
*END STEP
"""
    (tmp_path / "cube.inp").write_text(deck)
    r = run([find_exe("ccx"), "-i", "cube"], cwd=tmp_path, timeout=60)
    assert r.returncode == 0, r.stdout + r.stderr
    dat = (tmp_path / "cube.dat").read_text()
    ux = [float(line.split()[1]) for line in dat.splitlines()
          if line.strip() and line.split()[0] in {"2", "3", "6", "7"}]
    assert len(ux) == 4, dat
    assert all(abs(u - 0.001) < 1e-6 for u in ux), ux
