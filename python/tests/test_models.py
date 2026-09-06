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


def test_network_model_introspection_round_trip():
    """Every network accessor bound by #216 reads back what was declared."""
    m = coso.NetworkModel()
    assert m.num_nodes() == 0
    assert m.num_arcs() == 0

    s = m.add_node(15, "source")
    t = m.add_node(-15, "sink")
    mid = m.add_node()
    a = m.add_arc(s, mid, 7, 2, 20)
    b = m.add_arc(mid, t)

    assert m.num_nodes() == 3
    assert (m.node(s).supply, m.node(s).name) == (15, "source")
    assert (m.node(t).supply, m.node(t).name) == (-15, "sink")
    assert (m.node(mid).supply, m.node(mid).name) == (0, "")

    assert m.num_arcs() == 2
    arc_a = m.arc(a)
    assert (arc_a.tail, arc_a.head, arc_a.cost) == (s, mid, 7)
    assert (arc_a.lower_cap, arc_a.upper_cap) == (2, 20)
    arc_b = m.arc(b)
    assert (arc_b.tail, arc_b.head, arc_b.cost, arc_b.lower_cap) == (mid, t, 0, 0)
    assert arc_b.upper_cap == 2**31 - 1


def test_lotsizing_model_introspection_round_trip():
    """Every lot-sizing accessor bound by #216 reads back what was declared."""
    m = coso.LotSizingModel()
    assert m.num_periods() == 0
    assert m.num_products() == 0

    m.set_num_periods(3)
    p0 = m.add_product(100.0, 2.0, 1.5, 0.25)
    p1 = m.add_product(50.0, 1.0, 0.5, 0.75)
    m.set_demand(p0, 0, 10.0)
    m.set_demand(p1, 1, 5.0)
    m.set_capacity(0, 80.0)
    m.add_bom(p0, p1, 2.5)

    assert m.num_periods() == 3
    assert m.num_products() == 2
    prod = m.product(p0)
    assert (prod.setup_cost, prod.setup_time) == (100.0, 2.0)
    assert (prod.unit_production_cost, prod.holding_cost) == (1.5, 0.25)
    assert m.demands() == [[10.0, 0.0, 0.0], [0.0, 5.0, 0.0]]
    assert m.capacities() == [80.0, 0.0, 0.0]
    bom = m.bom()
    assert (bom[0].parent, bom[0].child, bom[0].quantity) == (p0, p1, 2.5)


def test_lotsizing_model_introspection_shows_the_call_order_traps():
    """set_num_periods wipes; a product added before it has an empty row."""
    m = coso.LotSizingModel()
    p = m.add_product(1.0, 0.0, 1.0, 1.0)
    assert m.demands() == [[]]
    assert m.capacities() == []

    m.set_num_periods(2)
    m.set_demand(p, 0, 42.0)
    m.set_capacity(1, 99.0)
    assert m.demands() == [[42.0, 0.0]]
    assert m.capacities() == [0.0, 99.0]

    # set_num_periods wipes demand and capacity, but keeps the products.
    m.set_num_periods(4)
    assert m.num_products() == 1
    assert m.demands() == [[0.0, 0.0, 0.0, 0.0]]
    assert m.capacities() == [0.0, 0.0, 0.0, 0.0]
