"""Repair playbooks (E3): a typed failure -> concrete actions the agent can take next.

Concrete actions, not reflection prompts: a scripted reflection prompt added nothing in arXiv 2609.03718, while
repair attempts with specific information raised success from 0.9% to 96.4% over ten attempts. The budget is ten
attempts per step; two identical failures in a row escalate to the stronger model (E4); the tenth ends the step with
a report.
"""
from __future__ import annotations

import re

BUDGET = 10
ESCALATE_AFTER = 2

PLAYBOOKS = [
    (r"thinner than the minimum", "gate.thin_wall",
     ["raise the thickness parameter of the thin feature", "remove or reduce the fillet next to the thin wall",
      "increase rib_thickness if the thin wall is a rib"]),
    (r"shorter than the minimum feature size", "gate.small_feature",
     ["increase the smallest fillet or chamfer above 0.5 mm, or remove it",
      "move features apart so no edge between them is shorter than the minimum"]),
    (r"solids; exactly one connected solid", "gate.disconnected",
     ["make every added feature touch or overlap the part (check its z or x against the plate it should sit on)",
      "remove the feature that floats"]),
    (r"outside the design space", "gate.design_space", ["reduce the dimension named in the bounding box message", "move the feature inward"]),
    (r"keep-out zone", "gate.keep_out", ["move or shrink the feature that intrudes into the keep-out box"]),
    (r"hole of diameter", "gate.small_hole", ["raise the hole diameter to at least the minimum, or remove the hole"]),
    (r"selects no edges", "compile.fillet_selects_nothing", ["widen the fillet region so it contains the edge midpoints",
                                                             "check edge_axis: the edges must be straight and parallel to it"]),
    (r"boolean (add|cut) failed", "compile.boolean", ["avoid coincident faces: offset the feature by 0.1 mm", "reduce the overlap between features"]),
    (r"outside physical bounds|outside \[", "graph.bounds", ["bring the value named in the message inside its bounds (describe_grammar or list_families)"]),
    (r"refers to unknown parameter", "graph.unknown_name", ["declare the parameter in the graph or use a number"]),
    (r"not an earlier feature", "graph.reference", ["put the target feature before the pattern, or fix its id"]),
    (r"inverted|nonpositive jacobian", "mesh.inverted", ["the mesher already retries with straight-sided elements; enlarge the smallest fillet"]),
    (r"meshing failed", "mesh.failed", ["enlarge the smallest feature, or coarsen the mesh size by 1.5x"]),
    (r"free to (move|rotate)", "setup.rigid_body", ["add a support that stops the motion named in the message",
                                                    "hold more directions on the existing support face"]),
    (r"which does not exist on this mesh", "setup.unknown_face", ["use a face name from list_families; a feature change may have removed the face"]),
    (r"acts on a fully held face", "setup.load_on_support", ["move the load to a face that is free to move"]),
    (r"not been accepted", "approval", ["show the spec card and ask the user to accept it"]),
    (r"infeasible|even the strongest design", "spec.infeasible",
     ["propose the smallest relaxation of the failing requirement (best achievable value is in the message)",
      "widen the free-parameter bounds if the family allows", "propose another part family or material"]),
]


def classify(message: str) -> dict:
    for pattern, kind, actions in PLAYBOOKS:
        if re.search(pattern, message, re.I):
            return {"kind": kind, "actions": actions}
    return {"kind": "unknown", "actions": ["read the message, change one thing, and retry; report if it repeats"]}


class RepairBudget:
    """Tracks attempts for one step: when to escalate the model and when to stop."""

    def __init__(self, step: str):
        self.step, self.attempts, self.history = step, 0, []

    def record(self, message: str) -> dict:
        self.attempts += 1
        c = classify(message)
        self.history.append(c["kind"])
        repeated = len(self.history) >= ESCALATE_AFTER and len(set(self.history[-ESCALATE_AFTER:])) == 1
        return {**c, "attempt": self.attempts, "budget": BUDGET, "escalate_model": repeated,
                "stop": self.attempts >= BUDGET,
                "next": "stop and report" if self.attempts >= BUDGET else ("retry with the stronger model" if repeated else "retry")}
