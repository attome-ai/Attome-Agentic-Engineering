"""E4: routes, fallback after two failures, tokens per step, and no invented cost."""
import pytest

from structura.agent import routing, studies as S

ROUTES = {"token_cap_per_study": 100, "prices_usd_per_million_tokens": {"priced": {"input": 2.0, "output": 10.0}},
          "routes": {"repair": {"models": ["cheap", "strong"], "basis": "x"}}}


def test_every_shipped_route_names_a_model():
    d = routing.load_routes()
    for step in ("plan", "spec_extraction", "repair"):
        assert routing.route(step, routes=d)["model"]


def test_fallback_after_two_failures_then_stop():
    assert routing.route("repair", 0, ROUTES)["model"] == "cheap"
    assert routing.route("repair", 1, ROUTES)["model"] == "cheap"
    second = routing.route("repair", 2, ROUTES)
    assert second["model"] == "strong" and second["switched"]
    assert routing.route("repair", 4, ROUTES)["exhausted"]
    with pytest.raises(KeyError):
        routing.route("unknown_step", routes=ROUTES)


def test_usage_totals_and_cost(tmp_path, monkeypatch):
    monkeypatch.setattr(S, "STUDIES", tmp_path)
    S.log("usage-test", "spec", "ok")
    routing.record_usage("usage-test", "repair", "priced", 50, 30)
    c = routing.study_cost("usage-test", ROUTES)
    assert (c["input_tokens"], c["output_tokens"]) == (50, 30)
    assert c["by_step"]["repair"]["calls"] == 1
    assert c["cost_usd"] == pytest.approx((50 * 2 + 30 * 10) / 1e6, abs=1e-4)
    assert not c["over_cap"]
    routing.record_usage("usage-test", "repair", "unpriced", 40, 10)
    c = routing.study_cost("usage-test", ROUTES)
    assert c["cost_usd"] is None and "unpriced" in c["cost_note"]  # a missing price is never guessed
    assert c["over_cap"]
    assert S.verify_log("usage-test")["ok"]  # usage is part of the tamper-evident log
    with pytest.raises(ValueError):
        routing.record_usage("usage-test", "repair", "priced", -1, 0)
