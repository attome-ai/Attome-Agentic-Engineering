"""H2: open the exported STL and GLB in Blender (an outside program) and measure what it reads.

Run inside Blender: blender.exe -b --factory-startup --python benchmarks/h2_blender_check.py -- FOLDER OUT.json
For each file: import with Blender's own importer, then report vertex and face counts, whether the mesh is closed
(manifold, every edge shared by exactly two faces), the volume (bmesh) and the bounding box, in the file's units.
"""
import json
import sys

import bmesh
import bpy


def measure():
    objs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bm = bmesh.new()
    for o in objs:
        tmp = bmesh.new()
        tmp.from_mesh(o.data)
        tmp.transform(o.matrix_world)
        m = bpy.data.meshes.new("tmp")
        tmp.to_mesh(m)
        bm.from_mesh(m)
        tmp.free()
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    xs = [v.co for v in bm.verts]
    lo = [min(c[i] for c in xs) for i in range(3)]
    hi = [max(c[i] for c in xs) for i in range(3)]
    open_edges = sum(1 for e in bm.edges if len(e.link_faces) != 2)
    out = {"objects": len(objs), "vertices": len(bm.verts), "faces": len(bm.faces), "open_or_nonmanifold_edges": open_edges,
           "volume": abs(bm.calc_volume()), "bbox_min": lo, "bbox_max": hi}
    bm.free()
    return out


def main():
    folder, out = sys.argv[sys.argv.index("--") + 1:][:2]
    res = {"blender": bpy.app.version_string}
    for name, imp in (("part.stl", lambda p: bpy.ops.wm.stl_import(filepath=p)),
                      ("part.glb", lambda p: bpy.ops.import_scene.gltf(filepath=p))):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        try:
            imp(f"{folder}/{name}")
            res[name] = measure()
        except Exception as e:  # noqa: BLE001
            res[name] = {"error": str(e)[:300]}
    with open(out, "w") as f:
        json.dump(res, f, indent=1)


main()
