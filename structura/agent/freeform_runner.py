"""Run model-written CadQuery code in its own process and export the solid to STEP (B1 free-form fallback).

Usage: python -I freeform_runner.py MODEL.py OUT.step
The code runs with `cq` (cadquery) imported and must assign the final solid to `result` (a cq.Workplane or a
cq.Shape). Exit 0 and write OUT.step on success; on failure print one JSON line {"error": kind, "message": ...} and
exit 1. The caller enforces the time limit and runs this from an empty working folder.
"""
import json
import sys
import traceback


def fail(kind: str, message: str) -> None:
    print(json.dumps({"error": kind, "message": message[-2000:]}))
    sys.exit(1)


def main():
    if len(sys.argv) != 3:
        fail("usage", "freeform_runner.py MODEL.py OUT.step")
    model, out = sys.argv[1], sys.argv[2]
    import cadquery as cq
    code = open(model, encoding="utf-8").read()
    ns = {"cq": cq, "__name__": "__freeform__"}
    try:
        exec(compile(code, "model.py", "exec"), ns)  # noqa: S102 - the point of this process: run the model's code, isolated
    except Exception:
        fail("code", traceback.format_exc(limit=3))
    result = ns.get("result")
    if result is None:
        fail("result", "the code must assign the final solid to `result`")
    try:
        shape = result.val() if isinstance(result, cq.Workplane) else result
        if isinstance(result, cq.Workplane) and len(result.vals()) > 1:
            shape = cq.Compound.makeCompound([v for v in result.vals() if isinstance(v, cq.Shape)])
        if not isinstance(shape, cq.Shape):
            fail("result", f"`result` is a {type(shape).__name__}, not a solid")
        if not shape.isValid():
            fail("geometry", "the solid is not valid (OCCT check failed)")
        cq.exporters.export(shape, out, exportType="STEP")
    except SystemExit:
        raise
    except Exception:
        fail("export", traceback.format_exc(limit=3))
    print(json.dumps({"ok": True, "volume_mm3": shape.Volume(), "solids": len(shape.Solids())}))


if __name__ == "__main__":
    main()
