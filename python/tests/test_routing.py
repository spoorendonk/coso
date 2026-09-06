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
