"""F2-T2: NAFEMS LE10 thick plate under pressure, solved with CalculiX at three mesh densities.

Geometry (quarter model, lengths in m, stresses in MPa): elliptic annulus with inner semi-axes 2.0 x 1.0 and
outer semi-axes 3.25 x 2.75, thickness 0.6 (z = -0.3 .. 0.3). Points: D = (2, 0) inner ellipse on y = 0,
C = (3.25, 0), A = (0, 1), B = (0, 2.75).
Material: E = 210000 MPa, nu = 0.3. Load: 1 MPa pressure on the upper face (z = +0.3).
Supports: face on x = 0 (ABA'B'): u_x = 0; face on y = 0 (DCD'C'): u_y = 0; outer curved face (BCB'C'):
u_x = u_y = 0; mid-plane line of the outer face (z = 0): u_z = 0.
Target: sigma_yy at point D (2, 0, 0.3) = -5.38 MPa.

gmsh and ccx run as subprocesses. Usage (from the repository root):
    micromamba run -n attome-ae python benchmarks/le10/le10.py
"""
from __future__ import annotations

import re
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from structura.tools import find_exe, run  # noqa: E402

REF = -5.38
WORK = Path(__file__).resolve().parent / "work"
LEVELS = [("coarse", 0.30), ("medium", 0.15), ("fine", 0.075)]
THREADS = 16

GEO = """SetFactory("OpenCASCADE");
Mesh.ElementOrder = 2;
Mesh.SecondOrderLinear = 0;
Mesh.HighOrderOptimize = 1;
Mesh.CharacteristicLengthMax = {lc};
Mesh.CharacteristicLengthMin = {lc_min};
Mesh.SaveGroupsOfNodes = 1;
Mesh.Algorithm3D = 1;
Disk(1) = {{0, 0, 0, 3.25, 2.75}};
Disk(2) = {{0, 0, 0, 2.0, 1.0}};
Rectangle(3) = {{0, 0, 0, 4, 4}};
BooleanDifference(4) = {{ Surface{{1}}; Delete; }}{{ Surface{{2}}; Delete; }};
BooleanIntersection(5) = {{ Surface{{4}}; Delete; }}{{ Surface{{3}}; Delete; }};
up[] = Extrude {{0, 0, 0.3}} {{ Surface{{5}}; }};
dn[] = Extrude {{0, 0, -0.3}} {{ Surface{{5}}; }};
Coherence;
e = 1e-6;
Physical Volume("EALL") = Volume{{:}};
Physical Surface("TOP") = Surface In BoundingBox{{-e, -e, 0.3 - e, 4, 4, 0.3 + e}};
Physical Surface("XZERO") = Surface In BoundingBox{{-e, -e, -0.3 - e, e, 4, 0.3 + e}};
Physical Surface("YZERO") = Surface In BoundingBox{{-e, -e, -0.3 - e, 4, e, 0.3 + e}};
outer[] = Surface In BoundingBox{{-e, -e, -0.3 - e, 3.25 + e, 2.75 + e, 0.3 + e}};
inner[] = Surface In BoundingBox{{-e, -e, -0.3 - e, 2.0 + e, 1.0 + e, 0.3 + e}};
xz[] = Surface In BoundingBox{{-e, -e, -0.3 - e, e, 4, 0.3 + e}};
yz[] = Surface In BoundingBox{{-e, -e, -0.3 - e, 4, e, 0.3 + e}};
top[] = Surface In BoundingBox{{-e, -e, 0.3 - e, 4, 4, 0.3 + e}};
bot[] = Surface In BoundingBox{{-e, -e, -0.3 - e, 4, 4, -0.3 + e}};
flat[] = Surface In BoundingBox{{-e, -e, -e, 4, 4, e}};
outer[] -= {{inner[], xz[], yz[], top[], bot[], flat[]}};
Physical Surface("OUTERALL") = outer[];
mid[] = Curve In BoundingBox{{-e, -e, -e, 3.25 + e, 2.75 + e, e}};
innc[] = Curve In BoundingBox{{-e, -e, -e, 2.0 + e, 1.0 + e, e}};
xl[] = Curve In BoundingBox{{-e, -e, -e, e, 4, e}};
yl[] = Curve In BoundingBox{{-e, -e, -e, 4, e, e}};
mid[] -= {{innc[], xl[], yl[]}};
Physical Curve("MIDLINE") = mid[];
Physical Point("D") = Point In BoundingBox{{2.0 - e, -e, 0.3 - e, 2.0 + e, e, 0.3 + e}};
"""

# CalculiX C3D10 face numbering by corner nodes (1-based local indices).
FACES = {1: (1, 2, 3), 2: (1, 4, 2), 3: (2, 4, 3), 4: (3, 4, 1)}


def parse_inp(text):
    """Return nodes {id: (x, y, z)}, element blocks [(type, elset, [[id, n1..]])], node sets {name: [ids]}."""
    nodes, blocks, nsets = {}, [], {}
    cur, mode = None, None
    for line in text.splitlines():
        s = line.strip()
        if not s or s.startswith("**"):
            continue
        if s.startswith("*"):
            key = s.upper()
            if key.startswith("*NODE") and "PRINT" not in key and "FILE" not in key:
                mode = "node"
            elif key.startswith("*ELEMENT"):
                typ = re.search(r"TYPE=([\w]+)", key).group(1)
                es = re.search(r"ELSET=([\w\-]+)", key)
                cur = (typ, es.group(1) if es else "", [])
                blocks.append(cur)
                mode = "elem"
            elif key.startswith("*NSET"):
                name = re.search(r"NSET=([\w\-]+)", key).group(1)
                cur = nsets.setdefault(name, [])
                mode = "nset"
            else:
                mode = None
            continue
        vals = [v for v in s.split(",") if v.strip()]
        if mode == "node":
            nodes[int(vals[0])] = tuple(float(v) for v in vals[1:4])
        elif mode == "elem":
            ints = [int(v) for v in vals]
            row = cur[2]
            if row and len(row[-1]) < expected_len(cur[0]):
                row[-1].extend(ints)
            else:
                row.append(ints)
        elif mode == "nset":
            cur.extend(int(v) for v in vals)
    return nodes, blocks, nsets


def expected_len(typ):
    return {"C3D10": 11, "C3D4": 5, "CPS6": 7, "STRI65": 7, "CPS3": 4, "T3D3": 4, "T3D2": 3}.get(typ.upper(), 0) or 10 ** 6


def surface_faces(tets, tris):
    """Map surface triangles to (tet id, CalculiX face number) through their corner nodes."""
    by_face = {}
    for t in tets:
        eid, n = t[0], t[1:]
        for fno, loc in FACES.items():
            by_face[frozenset(n[i - 1] for i in loc)] = (eid, fno)
    out = []
    for tri in tris:
        key = frozenset(tri[1:4])
        if key not in by_face:
            raise RuntimeError(f"surface triangle {tri[0]} has no matching tet face")
        out.append(by_face[key])
    return out


def write_deck(path, nodes, tets, nsets, top_faces):
    lines = ["*NODE"]
    lines += [f"{i},{x:.12g},{y:.12g},{z:.12g}" for i, (x, y, z) in sorted(nodes.items())]
    lines.append("*ELEMENT,TYPE=C3D10,ELSET=EALL")
    for t in tets:
        lines.append(",".join(str(v) for v in t[:8]) + ",")
        lines.append(",".join(str(v) for v in t[8:]))
    for name in ("XZERO", "YZERO", "OUTERALL", "MIDLINE", "D"):
        ids = sorted(set(nsets[name]))
        lines.append(f"*NSET,NSET={name}")
        lines += [",".join(str(v) for v in ids[i:i + 16]) for i in range(0, len(ids), 16)]
    lines.append("*SURFACE,NAME=STOP,TYPE=ELEMENT")
    lines += [f"{e},S{f}" for e, f in top_faces]
    lines += [
        "*MATERIAL,NAME=STEEL", "*ELASTIC", "210000,0.3",
        "*SOLID SECTION,ELSET=EALL,MATERIAL=STEEL",
        "*BOUNDARY", "XZERO,1,1", "YZERO,2,2", "OUTERALL,1,2", "MIDLINE,3,3",
        "*STEP", "*STATIC", "*DSLOAD", "STOP,P,1.0",
        "*NODE FILE", "U,S",
        "*END STEP", "",
    ]
    path.write_text("\n".join(lines))


def frd_stress(path, node):
    """Return (sxx, syy, szz, sxy, syz, szx) at a node from the STRESS block of an ASCII .frd file."""
    found, block = None, False
    for line in path.read_text().splitlines():
        if line.startswith(" -4"):
            block = line.split()[1].upper() == "STRESS"
            continue
        if block and line.startswith(" -1"):
            if int(line[3:13]) == node:
                found = tuple(float(line[13 + 12 * i: 25 + 12 * i]) for i in range(6))
        elif block and line.startswith(" -3"):
            block = False
    if found is None:
        raise RuntimeError(f"node {node} not in STRESS block")
    return found


def run_level(name, lc):
    d = WORK / name
    d.mkdir(parents=True, exist_ok=True)
    (d / "le10.geo").write_text(GEO.format(lc=lc, lc_min=lc / 4))
    t0 = time.perf_counter()
    r = run([find_exe("gmsh"), "le10.geo", "-3", "-format", "inp", "-o", "mesh.inp"], cwd=d, timeout=1800, threads=THREADS)
    t_mesh = time.perf_counter() - t0
    if r.returncode != 0 or not (d / "mesh.inp").exists():
        raise RuntimeError(f"gmsh failed for {name}:\n{r.stdout[-2000:]}\n{r.stderr[-2000:]}")
    nodes, blocks, nsets = parse_inp((d / "mesh.inp").read_text())
    tets = [row for typ, _, rows in blocks if typ.upper() == "C3D10" for row in rows]
    top_nodes = set(nsets["TOP"])
    top_tris = [row for typ, _, rows in blocks if typ.upper() == "CPS6" for row in rows
                if set(row[1:4]) <= top_nodes]
    if not top_tris:
        raise RuntimeError("no pressure faces found")
    used = {n for t in tets for n in t[1:]}
    nodes = {i: c for i, c in nodes.items() if i in used}
    top_faces = surface_faces(tets, top_tris)
    write_deck(d / "le10.inp", nodes, tets, nsets, top_faces)
    t0 = time.perf_counter()
    r = run([find_exe("ccx"), "-i", "le10"], cwd=d, timeout=3600, threads=THREADS)
    t_solve = time.perf_counter() - t0
    frd = d / "le10.frd"
    if r.returncode != 0 or not frd.exists() or "*WARNING" in r.stdout or "*ERROR" in r.stdout:
        raise RuntimeError(f"ccx failed or warned for {name}:\n{r.stdout[-3000:]}")
    node_d = sorted(set(nsets["D"]))
    if len(node_d) != 1:
        raise RuntimeError(f"point D maps to {len(node_d)} nodes")
    syy = frd_stress(frd, node_d[0])[1]
    return dict(level=name, lc=lc, nodes=len(nodes), elements=len(tets), dof=3 * len(nodes), syy=syy,
                err=100 * (syy - REF) / abs(REF), t_mesh=t_mesh, t_solve=t_solve)


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    res = [run_level(n, lc) for n, lc in LEVELS]
    hdr = "| Mesh | Size (m) | Nodes | C3D10 elements | DOF | sigma_yy at D (MPa) | Error vs -5.38 | Mesh time (s) | Solve time (s) |"
    print(hdr)
    print("|---|---|---|---|---|---|---|---|---|")
    for x in res:
        print(f"| {x['level']} | {x['lc']} | {x['nodes']} | {x['elements']} | {x['dof']} | {x['syy']:.3f} | "
              f"{x['err']:+.2f}% | {x['t_mesh']:.1f} | {x['t_solve']:.1f} |")
    return res


if __name__ == "__main__":
    main()
