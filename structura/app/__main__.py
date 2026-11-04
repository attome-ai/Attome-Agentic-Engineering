"""Hands-on workbench (I2 first release): a local web page to evaluate designs by hand with the real engine.

Usage: micromamba run -n attome-ae python -m structura.app [--port 8765] [--no-browser]
Pick a brief (any spec in docs/examples or a study's spec), move the parameter sliders and press Evaluate: the engine
builds, gates, meshes and solves the design (the same path the optimizer uses) and the page shows the field in 3D, the
metrics and every requirement with its margin. Python standard library only; listens on 127.0.0.1 only.
"""
from __future__ import annotations

import argparse
import json
import tempfile
import threading
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PAGE = Path(__file__).with_name("app.html")
THREE = ROOT / "data" / "vendor" / "three-0.149.0.min.js"
_lock = threading.Lock()


def specs() -> list[dict]:
    from structura import engine
    out = []
    for p in sorted((ROOT / "docs" / "examples").glob("*.json")):
        try:
            s = engine.call("spec", spec=str(p))
        except Exception:  # noqa: BLE001 - not every example is a design spec (e.g. a solve job)
            continue
        out.append({"file": p.name, "id": s["id"], "family": s["family"], "brief": s.get("brief", ""),
                    "free": s["free_params"], "requirements": s["requirements"], "objectives": s["objectives"]})
    return out


def evaluate(file: str, params: dict, mesh: float) -> dict:
    from structura.optimize.evaluate import evaluate as ev, load_spec
    path = ROOT / "docs" / "examples" / Path(file).name  # only the example folder: no arbitrary paths
    spec = load_spec(path)
    work = Path(tempfile.mkdtemp(prefix="structura_app_"))
    with _lock:  # one solve at a time: the page is for one person at a desk
        e = ev(spec, {k: float(v) for k, v in params.items()}, mesh_size=mesh, threads=8, workdir=work, bundle=True, use_cache=False)
    raw = e.raw
    res = {"status": e.status, "feasible": e.feasible, "metrics": raw.get("metrics"), "checks": raw.get("checks"),
           "gate": raw.get("gate"), "error": raw.get("error"), "manufacturing": raw.get("manufacturing")}
    if e.status == "ok":
        case = next(iter(raw["load_cases"]))
        skin = work / f"result_{case}" / "skin.json"
        if skin.exists():
            res["skin"] = json.loads(skin.read_text())
            res["case"] = case
    return res


class Handler(BaseHTTPRequestHandler):
    def _send(self, code: int, body: bytes, ctype: str):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _json(self, obj, code=200):
        self._send(code, json.dumps(obj).encode(), "application/json")

    def log_message(self, fmt, *args):  # quiet console
        pass

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            html = PAGE.read_text(encoding="utf-8").replace("__THREE__", THREE.read_text(encoding="utf-8"))
            self._send(200, html.encode(), "text/html; charset=utf-8")
        elif self.path == "/api/specs":
            self._json(specs())
        else:
            self._json({"error": "not found"}, 404)

    def do_POST(self):
        if self.path != "/api/evaluate":
            return self._json({"error": "not found"}, 404)
        try:
            body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            self._json(evaluate(body["file"], body.get("params", {}), float(body.get("mesh", 4.0))))
        except Exception as e:  # noqa: BLE001 - report to the page
            self._json({"status": "error", "error": str(e)}, 500)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--no-browser", action="store_true")
    a = ap.parse_args()
    srv = ThreadingHTTPServer(("127.0.0.1", a.port), Handler)
    url = f"http://127.0.0.1:{a.port}/"
    print(f"Attome AE workbench on {url} (Ctrl+C to stop)")
    if not a.no_browser:
        webbrowser.open(url)
    srv.serve_forever()


if __name__ == "__main__":
    main()
