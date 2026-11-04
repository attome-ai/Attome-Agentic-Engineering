"""Template: a part family defined as a saved design graph with named free parameters.

Templates never hand-code geometry; the graph JSON in structura/families/ is the family.
"""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import cadquery as cq

from structura.grammar.compile import CompiledPart, compile_graph
from structura.grammar.graph import DesignGraph, Param

FAMILIES = Path(__file__).resolve().parents[1] / "families"


@dataclass
class Template:
    graph: DesignGraph

    @classmethod
    def load(cls, name: str) -> "Template":
        path = FAMILIES / f"{name}.json"
        if not path.exists():
            raise FileNotFoundError(f"no part family {name!r} in {FAMILIES}")
        t = cls(DesignGraph.load(path))
        unused = set(t.graph.parameters) - t.graph.referenced_names()
        if unused:
            raise ValueError(f"family {name!r} declares parameters it never uses: {sorted(unused)}")
        return t

    @property
    def name(self) -> str:
        return self.graph.name

    @property
    def version(self) -> str:
        return self.graph.version

    @property
    def free(self) -> dict[str, Param]:
        return self.graph.parameters

    def defaults(self) -> dict[str, float]:
        return {k: p.default for k, p in self.free.items()}

    def build(self, params: dict[str, float] | None = None) -> CompiledPart:
        """Compile the family for the given parameters (missing ones take their defaults)."""
        return compile_graph(self.graph, params)

    def named_faces(self, part: CompiledPart) -> dict[str, list[cq.Face]]:
        return part.named_faces()


def list_families() -> list[str]:
    return sorted(p.stem for p in FAMILIES.glob("*.json"))
