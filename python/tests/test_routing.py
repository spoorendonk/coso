"""Tests for the COSO Python bindings (RoutingModel and shared types)."""

import coso


# =========================================================================
#  Shared types
# =========================================================================


class TestCoord:
    def test_constructor(self):
        c = coso.Coord(3.5, -1.2)
        assert c.x == 3.5
        assert c.y == -1.2

    def test_readwrite(self):
        c = coso.Coord(0.0, 0.0)
        c.x = 42.0
        c.y = -7.0
        assert c.x == 42.0
        assert c.y == -7.0

    def test_repr(self):
        c = coso.Coord(1.0, 2.0)
        r = repr(c)
        assert "Coord" in r
        assert "1.0" in r


class TestTimeWindow:
    def test_constructor(self):
        tw = coso.TimeWindow(10, 200)
        assert tw.start == 10
        assert tw.end == 200

    def test_readwrite(self):
        tw = coso.TimeWindow(0, 0)
        tw.start = 5
        tw.end = 100
        assert tw.start == 5
        assert tw.end == 100

    def test_repr(self):
        tw = coso.TimeWindow(0, 999)
        r = repr(tw)
        assert "TimeWindow" in r


class TestTimeLimit:
    def test_constructor(self):
        tl = coso.TimeLimit(30.0)
        assert tl.seconds == 30.0
        assert tl.work_units == 0.0

    def test_readwrite(self):
        tl = coso.TimeLimit(1.0)
        tl.seconds = 60.0
        tl.work_units = 12.5
        assert tl.seconds == 60.0
        assert tl.work_units == 12.5

    def test_repr(self):
        tl = coso.TimeLimit(5.0)
        r = repr(tl)
        assert "TimeLimit" in r


class TestCostParams:
    def test_defaults(self):
        cp = coso.CostParams()
        assert cp.fixed_cost == 0
        assert cp.unit_distance_cost == 1
        assert cp.unit_duration_cost == 0

    def test_readwrite(self):
        cp = coso.CostParams()
        cp.fixed_cost = 100
        cp.unit_distance_cost = 5
        cp.unit_duration_cost = 3
        assert cp.fixed_cost == 100
        assert cp.unit_distance_cost == 5
        assert cp.unit_duration_cost == 3

    def test_repr(self):
        cp = coso.CostParams()
        assert "CostParams" in repr(cp)


class TestResult:
    def test_default_result_properties(self):
        """Result objects come from solve(); verify property access works."""
        m = coso.RoutingModel()
        m.add_depot(0.0, 0.0)

        vt = coso.VehicleTypeParams()
        vt.capacity = [10]
        m.add_vehicle_type(1, vt)

        cp = coso.ClientParams()
        cp.demand = [1]
        m.add_client(1.0, 0.0, cp)

        r = m.solve(coso.TimeLimit(1.0))

        # These properties should all be accessible.
        assert isinstance(r.feasible, bool)
        assert isinstance(r.cost, (int, float))
        assert isinstance(r.elapsed_seconds, (int, float))
        assert isinstance(r.iterations, int)
        assert isinstance(r.work_ticks, int)
        assert isinstance(r.work_units, (int, float))
        assert isinstance(r.routes, list)
        assert isinstance(r.unserved, list)

    def test_repr(self):
        m = coso.RoutingModel()
        m.add_depot(0.0, 0.0)

        vt = coso.VehicleTypeParams()
        vt.capacity = [10]
        m.add_vehicle_type(1, vt)

        cp = coso.ClientParams()
        cp.demand = [1]
        m.add_client(1.0, 0.0, cp)

        r = m.solve(coso.TimeLimit(0.5))
        assert "Result" in repr(r)


# =========================================================================
#  VehicleTypeParams and ClientParams
# =========================================================================


class TestVehicleTypeParams:
    def test_defaults(self):
        vt = coso.VehicleTypeParams()
        assert vt.capacity == []
        assert vt.profile == 0

    def test_capacity(self):
        vt = coso.VehicleTypeParams()
        vt.capacity = [20]
        assert vt.capacity == [20]

    def test_cost_params(self):
        vt = coso.VehicleTypeParams()
        cp = coso.CostParams()
        cp.fixed_cost = 50
        vt.cost = cp
        assert vt.cost.fixed_cost == 50


class TestClientParams:
    def test_defaults(self):
        cp = coso.ClientParams()
        assert cp.demand == []
        assert cp.required is True

    def test_demand(self):
        cp = coso.ClientParams()
        cp.demand = [7]
        assert cp.demand == [7]

    def test_time_window(self):
        cp = coso.ClientParams()
        cp.tw = coso.TimeWindow(10, 200)
        assert cp.tw.start == 10
        assert cp.tw.end == 200

    def test_service_time(self):
        cp = coso.ClientParams()
        cp.service = 30
        assert cp.service == 30


class TestDepotParams:
    def test_defaults(self):
        dp = coso.DepotParams()
        # tw should exist (may be zero-initialized).
        _ = dp.tw

    def test_time_window(self):
        dp = coso.DepotParams()
        dp.tw = coso.TimeWindow(0, 1000)
        assert dp.tw.start == 0
        assert dp.tw.end == 1000


# =========================================================================
#  RoutingModel construction
# =========================================================================


class TestRoutingModelConstruction:
    def test_default_construct(self):
        m = coso.RoutingModel()
        assert m is not None

    def test_add_depot_coords(self):
        m = coso.RoutingModel()
        depot = m.add_depot(10.0, 20.0)
        assert depot >= 0

    def test_add_depot_id(self):
        m = coso.RoutingModel()
        depot = m.add_depot_id(0)
        assert depot >= 0

    def test_add_vehicle_type(self):
        m = coso.RoutingModel()
        vt = coso.VehicleTypeParams()
        vt.capacity = [15]
        idx = m.add_vehicle_type(4, vt)
        assert idx >= 0

    def test_add_client_coords(self):
        m = coso.RoutingModel()
        cp = coso.ClientParams()
        cp.demand = [5]
        c = m.add_client(1.0, 2.0, cp)
        assert c >= 0

    def test_add_client_id(self):
        m = coso.RoutingModel()
        c = m.add_client_id(42)
        assert c >= 0

    def test_add_client_group(self):
        m = coso.RoutingModel()
        g = m.add_client_group()
        assert g >= 0

    def test_set_distance_and_duration(self):
        m = coso.RoutingModel()
        m.set_distance(0, 1, 100)
        m.set_duration(0, 1, 50)

    def test_set_profile(self):
        m = coso.RoutingModel()
        m.set_profile(1)
        m.set_profile_distance(1, 0, 1, 200)
        m.set_profile_duration(1, 0, 1, 80)
        m.set_cost_matrix(0, 0, 1, 150)

    def test_warm_start(self):
        m = coso.RoutingModel()
        m.set_initial_routes([[1, 2, 3], [4, 5]])
        m.pin(1)


# =========================================================================
#  Solving
# =========================================================================


class TestSolveCVRP:
    def test_single_client(self):
        """Solve with a single client -- simplest feasible instance."""
        m = coso.RoutingModel()
        m.add_depot(0.0, 0.0)

        vt = coso.VehicleTypeParams()
        vt.capacity = [10]
        m.add_vehicle_type(1, vt)

        cp = coso.ClientParams()
        cp.demand = [1]
        m.add_client(1.0, 0.0, cp)

        r = m.solve(coso.TimeLimit(1.0))

        assert r.feasible
        assert r.cost > 0.0
        assert len(r.routes) == 1
        assert r.routes[0] == [0]  # client index 0
        assert r.unserved == []
        assert r.elapsed_seconds > 0.0

    def test_five_clients(self, small_cvrp):
        """Solve with 5 clients; all should be served."""
        r = small_cvrp.solve(coso.TimeLimit(2.0))

        assert r.feasible
        assert r.cost > 0.0
        assert len(r.routes) >= 1

        # All 5 clients must appear exactly once across routes.
        served = []
        for route in r.routes:
            served.extend(route)
        assert sorted(served) == [0, 1, 2, 3, 4]
        assert r.unserved == []

    def test_elapsed_and_iterations(self, small_cvrp):
        """Verify that elapsed time and iteration count are populated."""
        r = small_cvrp.solve(coso.TimeLimit(1.0))
        assert r.elapsed_seconds > 0.0
        assert r.iterations >= 0


class TestSolveVRPTW:
    def test_all_served_with_time_windows(self, vrptw_model):
        """Solve VRPTW; all clients should be served."""
        r = vrptw_model.solve(coso.TimeLimit(2.0))

        assert r.feasible
        assert r.cost > 0.0

        served = []
        for route in r.routes:
            served.extend(route)
        assert sorted(served) == [0, 1, 2]
        assert r.unserved == []


class TestSolveDistanceMatrix:
    def test_explicit_distances(self, distance_matrix_model):
        """Solve with explicit distance matrix overrides."""
        r = distance_matrix_model.solve(coso.TimeLimit(2.0))

        assert r.feasible
        served = []
        for route in r.routes:
            served.extend(route)
        assert sorted(served) == [0, 1]
        assert r.cost > 0.0


class TestSolveEdgeCases:
    def test_no_depot_infeasible(self):
        """Without a depot, solve should return infeasible."""
        m = coso.RoutingModel()

        vt = coso.VehicleTypeParams()
        vt.capacity = [10]
        m.add_vehicle_type(1, vt)

        cp = coso.ClientParams()
        cp.demand = [1]
        m.add_client(1.0, 0.0, cp)

        r = m.solve(coso.TimeLimit(1.0))
        assert not r.feasible

    def test_no_vehicles_infeasible(self):
        """Without vehicles, solve should return infeasible."""
        m = coso.RoutingModel()
        m.add_depot(0.0, 0.0)

        cp = coso.ClientParams()
        cp.demand = [1]
        m.add_client(1.0, 0.0, cp)

        r = m.solve(coso.TimeLimit(1.0))
        assert not r.feasible


class TestSolveInstance:
    def test_nonexistent_file(self):
        """solve_instance with a nonexistent file should return infeasible."""
        r = coso.solve_instance("nonexistent.vrp", coso.TimeLimit(1.0))
        assert not r.feasible
        assert r.cost >= 0.0


# =========================================================================
#  Module-level
# =========================================================================


class TestModule:
    def test_version(self):
        assert hasattr(coso, "__version__")
        assert isinstance(coso.__version__, str)
        assert len(coso.__version__) > 0


def test_routing_model_introspection_round_trip():
    """Every routing accessor bound by #216 reads back what was declared."""
    m = coso.RoutingModel()

    dp = coso.DepotParams()
    dp.tw = coso.TimeWindow(5, 500)
    m.add_depot(1.5, -2.5, dp)
    m.add_depot_id(42, dp)

    cp = coso.ClientParams()
    cp.demand = [3, 4]
    cp.pickup = [1, 2]
    cp.tw = coso.TimeWindow(10, 90)
    cp.service = 7
    cp.release_time = 8
    cp.prize = 9
    cp.required = False
    cp.group = 11
    cp.skills = ["crane"]
    cp.client_type = 2
    m.add_client(3.5, 4.5, cp)
    m.add_client_id(77, cp)

    vt = coso.VehicleTypeParams()
    vt.capacity = [50, 60]
    vt.max_duration = 480
    vt.profile = 1
    m.add_vehicle_type(6, vt)

    assert m.num_depots() == 2
    d0 = m.depot(0)
    assert (d0.x, d0.y, d0.has_coord, d0.explicit_id) == (1.5, -2.5, True, -1)
    assert (d0.params.tw.start, d0.params.tw.end) == (5, 500)
    # An explicit-id depot stores x = y = 0.0; has_coord is what tells it apart.
    d1 = m.depot(1)
    assert (d1.x, d1.y, d1.has_coord, d1.explicit_id) == (0.0, 0.0, False, 42)

    assert m.num_clients() == 2
    c0 = m.client(0)
    assert (c0.x, c0.y, c0.has_coord, c0.explicit_id) == (3.5, 4.5, True, -1)
    assert c0.params.demand == [3, 4]
    assert c0.params.pickup == [1, 2]
    assert (c0.params.tw.start, c0.params.tw.end) == (10, 90)
    assert c0.params.service == 7
    assert c0.params.release_time == 8
    assert c0.params.prize == 9
    assert c0.params.required is False
    assert c0.params.group == 11
    assert c0.params.skills == ["crane"]
    assert c0.params.client_type == 2
    c1 = m.client(1)
    assert (c1.x, c1.y, c1.has_coord, c1.explicit_id) == (0.0, 0.0, False, 77)

    assert m.num_vehicle_types() == 1
    v0 = m.vehicle_type(0)
    assert v0.count == 6
    assert v0.params.capacity == [50, 60]
    assert v0.params.max_duration == 480
    assert v0.params.profile == 1


def test_routing_model_introspection_of_matrices_and_warm_start():
    """The matrix log, requests, groups and pins read back as stored."""
    m = coso.RoutingModel()

    p = m.add_pickup(1.0, 1.0)
    d = m.add_delivery(2.0, 2.0)
    m.add_request(p, d)
    # add_pickup / add_delivery are aliases for add_client: the role is not
    # stored, only the pairing.
    assert m.requests() == [(p, d)]
    assert m.client(p).params.demand == []
    assert m.client(d).params.demand == []

    assert m.num_client_groups() == 0
    m.add_client_group()
    assert m.num_client_groups() == 1

    m.set_distance(0, 1, 10)
    m.set_duration(0, 1, 20)
    m.set_cost_matrix(1, 0, 1, 70)
    # The setters are an append-only log: a repeat appends, last one wins.
    m.set_distance(0, 1, 11)

    dist = m.distance_entries()
    assert len(dist) == 2
    assert (dist[0].profile, dist[0].from_node, dist[0].to_node, dist[0].value) == (0, 0, 1, 10)
    assert dist[1].value == 11
    assert [e.value for e in m.duration_entries()] == [20]
    cost = m.cost_entries()
    assert (cost[0].profile, cost[0].from_node, cost[0].to_node, cost[0].value) == (1, 0, 1, 70)

    m.set_initial_routes([[0, 1], [], [2]])
    assert m.initial_routes() == [[0, 1], [], [2]]

    # pin() appends with no dedup and no range check.
    assert m.pinned() == []
    m.pin(2)
    m.pin(2)
    m.pin(9999)
    assert m.pinned() == [2, 2, 9999]
