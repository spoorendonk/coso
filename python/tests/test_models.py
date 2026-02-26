"""Tests for non-routing model bindings (NetworkModel / LotSizingModel)."""

import coso


def test_result_exposes_new_sections():
    m = coso.NetworkModel()
    s = m.add_node(5, "s")
    t = m.add_node(-5, "t")
    m.add_arc(s, t, 2, 0, 5)

    r = m.solve(coso.TimeLimit(1.0))

    assert isinstance(r.flows, list)
    assert isinstance(r.production, list)
    assert isinstance(r.inventory, list)


def test_network_model_solve_and_flows():
    m = coso.NetworkModel()
    s = m.add_node(5, "source")
    mid = m.add_node(0, "mid")
    t = m.add_node(-5, "sink")

    m.add_arc(s, mid, 1, 0, 5)
    m.add_arc(mid, t, 1, 0, 5)
    m.add_arc(s, t, 5, 0, 5)

    r = m.solve(coso.TimeLimit(1.0))

    assert r.feasible
    assert r.cost == 10.0
    assert len(r.flows) == 1
    assert len(r.flows[0]) >= 1
    assert r.flows[0][0].flow > 0


def test_network_model_deterministic_work_units():
    m = coso.NetworkModel()
    s = m.add_node(5, "s")
    t = m.add_node(-5, "t")
    m.add_arc(s, t, 2, 0, 5)

    r1 = m.solve(coso.TimeLimit(1.0, 0.05))
    r2 = m.solve(coso.TimeLimit(1.0, 0.05))

    assert r1.work_ticks > 0
    assert r1.work_ticks == r2.work_ticks
    assert r1.work_units == r2.work_units


def test_lotsizing_model_solve():
    m = coso.LotSizingModel()
    m.set_num_periods(3)
    p = m.add_product(100.0, 2.0, 1.0, 2.0)

    m.set_demand(p, 0, 10.0)
    m.set_demand(p, 1, 15.0)
    m.set_demand(p, 2, 20.0)

    m.set_capacity(0, 80.0)
    m.set_capacity(1, 80.0)
    m.set_capacity(2, 80.0)

    r = m.solve(coso.TimeLimit(1.0))

    assert r.feasible
    assert len(r.production) == 1
    assert len(r.production[0]) == 3
    assert len(r.inventory) == 1
    assert len(r.inventory[0]) == 3


def test_lotsizing_model_deterministic_work_units():
    m = coso.LotSizingModel()
    m.set_num_periods(3)
    p = m.add_product(100.0, 2.0, 1.0, 2.0)
    m.set_demand(p, 0, 10.0)
    m.set_demand(p, 1, 15.0)
    m.set_demand(p, 2, 20.0)
    m.set_capacity(0, 80.0)
    m.set_capacity(1, 80.0)
    m.set_capacity(2, 80.0)

    r1 = m.solve(coso.TimeLimit(1.0, 0.05))
    r2 = m.solve(coso.TimeLimit(1.0, 0.05))

    assert r1.work_ticks > 0
    assert r1.work_ticks == r2.work_ticks
    assert r1.work_units == r2.work_units
