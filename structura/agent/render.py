"""Six-view render of a part for the agent's visual review (B1): front, back, left, right, top and an isometric view,
flat-shaded, with the overall size in mm. Offline (matplotlib, Agg backend); reads the engine's STL.

Usage: python -m structura.agent.render PART.stl OUT.png [--title TEXT]
"""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

import numpy as np


def read_stl(path: str | Path) -> np.ndarray:
    """Triangles as an (n, 3, 3) array; binary or ASCII STL."""
    data = Path(path).read_bytes()
    if len(data) >= 84:
        n = struct.unpack_from("<I", data, 80)[0]
        if 84 + 50 * n == len(data):
            rec = np.frombuffer(data, dtype=np.dtype([("n", "<f4", 3), ("v", "<f4", (3, 3)), ("a", "<u2")]), count=n, offset=84)
            return rec["v"].astype(float)
    vals = [list(map(float, ln.split()[1:4])) for ln in data.decode("ascii", "ignore").splitlines() if ln.strip().startswith("vertex")]
    return np.array(vals, dtype=float).reshape(-1, 3, 3)


VIEWS = [("Front (-y)", 0, -90), ("Back (+y)", 0, 90), ("Left (-x)", 0, 180), ("Right (+x)", 0, 0), ("Top (+z)", 90, -90),
         ("Isometric", 28, -55)]


def render(stl: str | Path, out: str | Path, title: str = "") -> Path:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    tri = read_stl(stl)
    lo, hi = tri.reshape(-1, 3).min(0), tri.reshape(-1, 3).max(0)
    size = hi - lo
    centre, half = (lo + hi) / 2, size.max() / 2 * 1.05
    nrm = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    nrm /= np.linalg.norm(nrm, axis=1, keepdims=True) + 1e-12
    fig = plt.figure(figsize=(12, 8), dpi=110)
    for k, (name, elev, azim) in enumerate(VIEWS):
        ax = fig.add_subplot(2, 3, k + 1, projection="3d")
        e, a = np.radians(elev), np.radians(azim)
        view = np.array([np.cos(e) * np.cos(a), np.cos(e) * np.sin(a), np.sin(e)])  # towards the camera
        light = view + np.array([0.2, -0.3, 0.5])
        light /= np.linalg.norm(light)
        shade = 0.35 + 0.6 * np.clip(np.abs(nrm @ light), 0, 1)
        col = np.stack([0.25 + 0.5 * shade, 0.45 + 0.45 * shade, 0.42 + 0.45 * shade, np.ones_like(shade)], axis=1).clip(0, 1)
        pc = Poly3DCollection(tri, facecolors=col, edgecolors="none")
        ax.add_collection3d(pc)
        for i, setter in enumerate((ax.set_xlim, ax.set_ylim, ax.set_zlim)):
            setter(centre[i] - half, centre[i] + half)
        ax.set_box_aspect((1, 1, 1))
        ax.view_init(elev=elev, azim=azim)
        ax.set_axis_off()
        ax.set_title(name, fontsize=10)
    fig.suptitle((title + "  " if title else "") + f"size {size[0]:.1f} x {size[1]:.1f} x {size[2]:.1f} mm (x, y, z), {len(tri)} triangles",
                 fontsize=11)
    fig.tight_layout()
    out = Path(out)
    out.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out)
    plt.close(fig)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("stl")
    ap.add_argument("out")
    ap.add_argument("--title", default="")
    a = ap.parse_args()
    print(render(a.stl, a.out, a.title))


if __name__ == "__main__":
    main()
