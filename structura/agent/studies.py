"""Study store shared by the MCP tools: one folder per study under runs/studies/<id>/.

A study holds versioned specs (spec.v<n>.json), an acceptance record (the approval gate), the Optuna storage of its
optimization runs, steering and verification records, and a step log that the agent timeline (G6) reads.
"""
from __future__ import annotations

import json
import re
import threading
import time
import uuid
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
STUDIES = ROOT / "runs" / "studies"
_lock = threading.Lock()


def now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def folder(study_id: str) -> Path:
    if not re.fullmatch(r"[a-z0-9][a-z0-9-]{0,63}", study_id):
        raise ValueError(f"bad study id {study_id!r}")
    return STUDIES / study_id


def new_id(spec_id: str) -> str:
    base = re.sub(r"[^a-z0-9-]+", "-", spec_id.lower()).strip("-")[:40] or "study"
    return f"{base}-{uuid.uuid4().hex[:6]}"


def log(study_id: str, step: str, outcome: str, **detail) -> None:
    """Append one step to the study's timeline (G6): what the agent did, when, and how it went."""
    d = folder(study_id)
    d.mkdir(parents=True, exist_ok=True)
    with _lock:
        prev = "0" * 64
        p = d / "timeline.jsonl"
        if p.exists():
            lines = [ln for ln in p.read_text(encoding="utf-8").splitlines() if ln.strip()]
            if lines:
                prev = json.loads(lines[-1]).get("hash", prev)
        entry = {"time": now(), "t": time.time(), "step": step, "outcome": outcome, **detail, "prev": prev}
        entry = json.loads(json.dumps(entry, default=str))  # hash exactly what is stored
        entry["hash"] = _entry_hash(entry)
        with open(p, "a", encoding="utf-8") as f:
            f.write(json.dumps(entry) + "\n")


def _entry_hash(entry: dict) -> str:
    """SHA-256 of the entry without its own hash: chains the audit log (I1), so an edited or removed entry is detectable."""
    import hashlib
    body = {k: v for k, v in entry.items() if k != "hash"}
    return hashlib.sha256(json.dumps(body, sort_keys=True).encode()).hexdigest()


def verify_log(study_id: str) -> dict:
    """Walk the chain: every entry's hash matches its content and points at the previous entry."""
    p = folder(study_id) / "timeline.jsonl"
    if not p.exists():
        return {"ok": True, "entries": 0}
    prev = "0" * 64
    rows = [json.loads(ln) for ln in p.read_text(encoding="utf-8").splitlines() if ln.strip()]
    for i, e in enumerate(rows):
        if "hash" not in e:
            return {"ok": False, "entries": len(rows), "broken_at": i, "reason": "entry without a hash (written before chaining)"}
        if e.get("prev") != prev:
            return {"ok": False, "entries": len(rows), "broken_at": i, "reason": "does not point at the previous entry (removed or reordered)"}
        if _entry_hash(e) != e["hash"]:
            return {"ok": False, "entries": len(rows), "broken_at": i, "reason": "content does not match its hash (edited)"}
        prev = e["hash"]
    return {"ok": True, "entries": len(rows), "head": prev}


def timeline(study_id: str) -> list[dict]:
    p = folder(study_id) / "timeline.jsonl"
    if not p.exists():
        return []
    return [json.loads(line) for line in p.read_text(encoding="utf-8").splitlines() if line.strip()]


def latest_spec_path(study_id: str) -> Path:
    d = folder(study_id)
    versions = sorted(d.glob("spec.v*.json"), key=lambda p: int(re.search(r"v(\d+)", p.name).group(1)))
    if not versions:
        raise FileNotFoundError(f"study {study_id} has no spec")
    return versions[-1]


def save_spec(study_id: str, spec: dict) -> Path:
    d = folder(study_id)
    d.mkdir(parents=True, exist_ok=True)
    n = len(list(d.glob("spec.v*.json"))) + 1
    p = d / f"spec.v{n}.json"
    p.write_text(json.dumps(spec, indent=2), encoding="utf-8")
    (d / "acceptance.json").unlink(missing_ok=True)  # a new spec version needs a new acceptance
    return p


def accept(study_id: str, by: str, note: str = "") -> dict:
    rec = {"spec": latest_spec_path(study_id).name, "accepted_by": by, "time": now(), "note": note}
    (folder(study_id) / "acceptance.json").write_text(json.dumps(rec, indent=2), encoding="utf-8")
    return rec


def accepted(study_id: str) -> bool:
    p = folder(study_id) / "acceptance.json"
    if not p.exists():
        return False
    return json.loads(p.read_text()).get("spec") == latest_spec_path(study_id).name
