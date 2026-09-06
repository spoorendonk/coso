#include <catch2/catch_test_macros.hpp>

#include <climits>
#include <model/assignment_model.h>
#include <model/lotsizing_model.h>
#include <model/network_model.h>
#include <model/packing_model.h>
#include <model/routing_model.h>
#include <model/schedule_model.h>
#include <string>
#include <utility>
#include <vector>

// =========================================================================
//  API contract tests
//
//  These tests verify that every public method on each model class compiles,
//  links, and can be called without crashing.  They do NOT test solver
//  correctness — that comes later once engines are implemented.
// =========================================================================

// --------------------------------------------------------------------------
//  Shared types
// --------------------------------------------------------------------------

TEST_CASE("Result default-constructs to infeasible with zero cost", "[types]") {
    coso::Result r;
    REQUIRE_FALSE(r.feasible());
    REQUIRE(r.cost() == 0.0);
    REQUIRE(r.elapsed_seconds() == 0.0);
    REQUIRE(r.iterations() == 0);
    REQUIRE(r.work_ticks() == 0);
    REQUIRE(r.work_units() == 0.0);
    REQUIRE(r.routes().empty());
    REQUIRE(r.unserved().empty());
    REQUIRE(r.schedule().empty());
    REQUIRE(r.makespan() == 0);
    REQUIRE(r.assignments().empty());
    REQUIRE(r.unassigned().empty());
    REQUIRE(r.bins().empty());
    REQUIRE(r.num_bins() == 0);
    REQUIRE(r.flows().empty());
}

TEST_CASE("TimeLimit stores seconds", "[types]") {
    coso::TimeLimit tl(30.0);
    REQUIRE(tl.seconds == 30.0);
}

TEST_CASE("CostParams has sensible defaults", "[types]") {
    coso::CostParams cp;
    REQUIRE(cp.fixed_cost == 0);
    REQUIRE(cp.unit_distance_cost == 1);
    REQUIRE(cp.unit_duration_cost == 0);
}

// --------------------------------------------------------------------------
//  RoutingModel
// --------------------------------------------------------------------------

TEST_CASE("RoutingModel can be default-constructed", "[routing]") {
    coso::RoutingModel m;
    (void)m;
}

TEST_CASE("RoutingModel add_depot with coordinates", "[routing]") {
    coso::RoutingModel m;
    int depot = m.add_depot(10.0, 20.0);
    REQUIRE(depot >= 0);
}

TEST_CASE("RoutingModel add_depot with explicit id", "[routing]") {
    coso::RoutingModel m;
    int depot = m.add_depot(0, coso::DepotParams{.tw = {0, 1000}});
    REQUIRE(depot >= 0);
}

TEST_CASE("RoutingModel add_vehicle_type", "[routing]") {
    coso::RoutingModel m;
    int vt = m.add_vehicle_type(4, {.capacity = {15}});
    REQUIRE(vt >= 0);
}

TEST_CASE("RoutingModel add_client with coordinates", "[routing]") {
    coso::RoutingModel m;
    int c = m.add_client(1.0, 2.0, {.demand = {5}});
    REQUIRE(c >= 0);
}

TEST_CASE("RoutingModel add_client with explicit id", "[routing]") {
    coso::RoutingModel m;
    int c = m.add_client(42);
    REQUIRE(c >= 0);
}

TEST_CASE("RoutingModel pickup-delivery workflow", "[routing]") {
    coso::RoutingModel m;
    m.add_depot(0.0, 0.0);
    int p = m.add_pickup(1.0, 0.0, {.pickup = {3}});
    int d = m.add_delivery(2.0, 0.0, {.demand = {3}});
    m.add_request(p, d);
    // add_pickup_delivery is an alias
    m.add_pickup_delivery(p, d);
}

TEST_CASE("RoutingModel client groups", "[routing]") {
    coso::RoutingModel m;
    int g = m.add_client_group();
    REQUIRE(g >= 0);
}

TEST_CASE("RoutingModel distance and duration setters", "[routing]") {
    coso::RoutingModel m;
    m.set_distance(0, 1, 100);
    m.set_duration(0, 1, 50);
    m.set_profile(1);
    m.set_profile_distance(1, 0, 1, 200);
    m.set_profile_duration(1, 0, 1, 80);
    m.set_cost_matrix(0, 0, 1, 150);
}

TEST_CASE("RoutingModel warm start and pin", "[routing]") {
    coso::RoutingModel m;
    m.set_initial_routes({{1, 2, 3}, {4, 5}});
    m.pin(1);
}

TEST_CASE("RoutingModel solve returns a Result", "[routing]") {
    coso::RoutingModel m;
    m.add_depot(0.0, 0.0);
    m.add_vehicle_type(1, {.capacity = {10}});
    m.add_client(1.0, 0.0, {.demand = {1}});
    coso::Result r = m.solve(coso::TimeLimit(1.0));
    REQUIRE(r.cost() >= 0.0);
    // With a real implementation, the single client should be served.
    REQUIRE(r.feasible());
    REQUIRE(r.routes().size() == 1);
    REQUIRE(r.routes()[0].size() == 1);
    REQUIRE(r.routes()[0][0] == 0);  // client index 0
    REQUIRE(r.unserved().empty());
    REQUIRE(r.elapsed_seconds() > 0.0);
    REQUIRE(r.work_ticks() > 0);
    REQUIRE(r.work_units() > 0.0);
}

TEST_CASE("Free function solve(instance_path, tl) links", "[routing]") {
    // Nonexistent file should return empty/infeasible result (no crash).
    coso::Result r = coso::solve("nonexistent.vrp", coso::TimeLimit(1.0));
    REQUIRE(r.cost() >= 0.0);
    REQUIRE_FALSE(r.feasible());
}

TEST_CASE("RoutingModel solves small CVRP", "[routing][integration]") {
    // Small CVRP instance: 1 depot at origin, 4 clients in a square.
    // Capacity 10, each client demands 3.  One vehicle of capacity 10
    // can serve at most 3 clients.  Two vehicles needed.
    coso::RoutingModel m;
    m.add_depot(0.0, 0.0);
    m.add_vehicle_type(2, {.capacity = {10}});

    m.add_client(10.0, 0.0, {.demand = {3}});
    m.add_client(10.0, 10.0, {.demand = {3}});
    m.add_client(0.0, 10.0, {.demand = {3}});
    m.add_client(20.0, 0.0, {.demand = {3}});

    coso::Result r = m.solve(coso::TimeLimit(2.0));

    REQUIRE(r.feasible());
    // All 4 clients should be served.
    int total_served = 0;
    for (auto const& route : r.routes()) {
        total_served += static_cast<int>(route.size());
    }
    REQUIRE(total_served == 4);
    REQUIRE(r.unserved().empty());
    REQUIRE(r.cost() > 0.0);
    REQUIRE(r.elapsed_seconds() > 0.0);
    REQUIRE(r.iterations() >= 0);
}

TEST_CASE("RoutingModel with explicit distances", "[routing][integration]") {
    // 1 depot, 2 clients, explicit distances.
    coso::RoutingModel m;
    m.add_depot(0.0, 0.0);
    m.add_vehicle_type(1, {.capacity = {20}});

    m.add_client(0.0, 0.0, {.demand = {5}});  // client 0
    m.add_client(0.0, 0.0, {.demand = {5}});  // client 1

    // Node 0 = depot, node 1 = client 0, node 2 = client 1
    // Set explicit distances (override Euclidean which would all be 0).
    m.set_distance(0, 1, 10);
    m.set_distance(1, 0, 10);
    m.set_distance(0, 2, 20);
    m.set_distance(2, 0, 20);
    m.set_distance(1, 2, 15);
    m.set_distance(2, 1, 15);

    coso::Result r = m.solve(coso::TimeLimit(2.0));

    REQUIRE(r.feasible());
    // Both clients should be served.
    int total_served = 0;
    for (auto const& route : r.routes()) {
        total_served += static_cast<int>(route.size());
    }
    REQUIRE(total_served == 2);
    REQUIRE(r.cost() > 0.0);
}

TEST_CASE("RoutingModel deterministic work units with work limit", "[routing][work_units]") {
    coso::RoutingModel m;
    m.add_depot(0.0, 0.0);
    m.add_vehicle_type(2, {.capacity = {10}});
    m.add_client(10.0, 0.0, {.demand = {3}});
    m.add_client(10.0, 10.0, {.demand = {3}});
    m.add_client(0.0, 10.0, {.demand = {3}});
    m.add_client(20.0, 0.0, {.demand = {3}});

    coso::Result r1 = m.solve(coso::TimeLimit(0.0, 0.05));
    coso::Result r2 = m.solve(coso::TimeLimit(0.0, 0.05));

    REQUIRE(r1.work_ticks() > 0);
    REQUIRE(r1.work_ticks() == r2.work_ticks());
    REQUIRE(r1.work_units() == r2.work_units());
}

TEST_CASE("RoutingModel no depot returns empty result", "[routing]") {
    coso::RoutingModel m;
    m.add_vehicle_type(1, {.capacity = {10}});
    m.add_client(1.0, 0.0, {.demand = {1}});
    coso::Result r = m.solve(coso::TimeLimit(1.0));
    // No depot: cannot solve.
    REQUIRE_FALSE(r.feasible());
}

TEST_CASE("RoutingModel no vehicle type returns empty result", "[routing]") {
    coso::RoutingModel m;
    m.add_depot(0.0, 0.0);
    m.add_client(1.0, 0.0, {.demand = {1}});
    coso::Result r = m.solve(coso::TimeLimit(1.0));
    // No vehicles: cannot solve.
    REQUIRE_FALSE(r.feasible());
}

// --------------------------------------------------------------------------
//  NetworkModel
// --------------------------------------------------------------------------

TEST_CASE("NetworkModel can be default-constructed", "[network]") {
    coso::NetworkModel m;
    (void)m;
}

TEST_CASE("NetworkModel add nodes and arcs", "[network]") {
    coso::NetworkModel m;
    int src = m.add_node(5, "src");
    int mid = m.add_node(0, "mid");
    int dst = m.add_node(-5, "dst");
    REQUIRE(src == 0);
    REQUIRE(mid == 1);
    REQUIRE(dst == 2);

    int a0 = m.add_arc(src, mid, 1, 0, 5);
    int a1 = m.add_arc(mid, dst, 1, 0, 5);
    REQUIRE(a0 == 0);
    REQUIRE(a1 == 1);
}

TEST_CASE("NetworkModel solve returns flow result", "[network]") {
    coso::NetworkModel m;
    int src = m.add_node(5, "src");
    int dst = m.add_node(-5, "dst");
    m.add_arc(src, dst, 2, 0, 5);

    coso::Result r = m.solve(coso::TimeLimit(1.0));
    REQUIRE(r.feasible());
    REQUIRE(r.cost() == 10.0);
    REQUIRE_FALSE(r.flows().empty());
    REQUIRE(r.work_ticks() > 0);
    REQUIRE(r.work_units() > 0.0);
}

// --------------------------------------------------------------------------
//  LotSizingModel
// --------------------------------------------------------------------------

TEST_CASE("LotSizingModel can be default-constructed", "[lotsizing]") {
    coso::LotSizingModel m;
    (void)m;
}

TEST_CASE("LotSizingModel add products and demand", "[lotsizing]") {
    coso::LotSizingModel m;
    m.set_num_periods(3);
    int p0 = m.add_product(100.0, 2.0, 1.0, 2.0);
    REQUIRE(p0 == 0);
    m.set_demand(p0, 0, 10.0);
    m.set_demand(p0, 1, 15.0);
    m.set_demand(p0, 2, 20.0);
    m.set_capacity(0, 50.0);
    m.set_capacity(1, 50.0);
    m.set_capacity(2, 50.0);
}

TEST_CASE("LotSizingModel solve returns typed production output", "[lotsizing]") {
    coso::LotSizingModel m;
    m.set_num_periods(3);
    int p0 = m.add_product(100.0, 2.0, 1.0, 2.0);
    m.set_demand(p0, 0, 10.0);
    m.set_demand(p0, 1, 15.0);
    m.set_demand(p0, 2, 20.0);
    m.set_capacity(0, 50.0);
    m.set_capacity(1, 50.0);
    m.set_capacity(2, 50.0);

    coso::Result r = m.solve(coso::TimeLimit(1.0));
    REQUIRE(r.cost() >= 0.0);
    REQUIRE_FALSE(r.production().empty());
    REQUIRE(r.production().size() == 1);
    REQUIRE(r.production()[0].size() == 3);
    REQUIRE(r.work_ticks() > 0);
    REQUIRE(r.work_units() > 0.0);
}

// --------------------------------------------------------------------------
//  ScheduleModel
// --------------------------------------------------------------------------

TEST_CASE("ScheduleModel can be default-constructed", "[scheduling]") {
    coso::ScheduleModel m;
    (void)m;
}

TEST_CASE("ScheduleModel add_machine", "[scheduling]") {
    coso::ScheduleModel m;
    int mach = m.add_machine({.name = "M1"});
    REQUIRE(mach >= 0);
}

TEST_CASE("ScheduleModel add_job and add_operation", "[scheduling]") {
    coso::ScheduleModel m;
    int j = m.add_job({.name = "Job0", .weight = 2});
    REQUIRE(j >= 0);
    int op = m.add_operation(j, {.machine = 0, .duration = 10});
    REQUIRE(op >= 0);
}

TEST_CASE("ScheduleModel FJSP flexible operations", "[scheduling]") {
    coso::ScheduleModel m;
    m.add_machine({.name = "M1"});
    m.add_machine({.name = "M2"});
    int j = m.add_job();
    int op = m.add_operation(j, {
                                    .eligible_machines = {0, 1},
                                    .durations_per_machine = {5, 8},
                                });
    REQUIRE(op >= 0);
}

TEST_CASE("ScheduleModel resource constraints (RCPSP)", "[scheduling]") {
    coso::ScheduleModel m;
    int res = m.add_resource(3);
    REQUIRE(res >= 0);
    int j = m.add_job();
    int op = m.add_operation(j, {.duration = 5});
    m.set_resource_usage(op, res, 2);
}

TEST_CASE("ScheduleModel precedence constraints", "[scheduling]") {
    coso::ScheduleModel m;
    int j = m.add_job();
    int op1 = m.add_operation(j, {.duration = 3});
    int op2 = m.add_operation(j, {.duration = 4});
    m.add_precedence(op1, op2);
}

TEST_CASE("ScheduleModel objectives", "[scheduling]") {
    coso::ScheduleModel m;
    m.set_objective(coso::ScheduleObjective::Makespan);
    m.set_objective(coso::ScheduleObjective::TotalWeightedTardiness);
    m.set_objective(coso::ScheduleObjective::TotalFlowTime);
    m.minimize_makespan();
}

TEST_CASE("ScheduleModel warm start", "[scheduling]") {
    coso::ScheduleModel m;
    m.set_initial_schedule({{0, 0}, {1, 5}});
}

TEST_CASE("ScheduleModel solve returns a Result", "[scheduling]") {
    coso::ScheduleModel m;
    m.add_machine();
    int j = m.add_job();
    m.add_operation(j, {.machine = 0, .duration = 10});
    coso::Result r = m.solve(coso::TimeLimit(1.0));
    REQUIRE(r.cost() >= 0.0);
    REQUIRE(r.work_ticks() > 0);
    REQUIRE(r.work_units() > 0.0);
}

TEST_CASE("Free function solve_jsp links", "[scheduling]") {
    coso::Result r = coso::solve_jsp("nonexistent.txt", coso::TimeLimit(1.0));
    REQUIRE(r.cost() >= 0.0);
}

// --------------------------------------------------------------------------
//  AssignmentModel
// --------------------------------------------------------------------------

TEST_CASE("AssignmentModel can be default-constructed", "[assignment]") {
    coso::AssignmentModel m;
    (void)m;
}

TEST_CASE("AssignmentModel add_shift_type and add_employee", "[assignment]") {
    coso::AssignmentModel m;
    int s = m.add_shift_type({.name = "Morning", .start_hour = 6, .end_hour = 14});
    REQUIRE(s >= 0);
    int e = m.add_employee({.name = "Alice", .skills = {"ICU"}});
    REQUIRE(e >= 0);
}

TEST_CASE("AssignmentModel planning horizon and demand", "[assignment]") {
    coso::AssignmentModel m;
    int s = m.add_shift_type({.name = "Day"});
    m.set_horizon(7);
    m.add_demand(s, 0, {.min_employees = 2});
    m.add_demand(s, {.min_employees = 1});  // all days
}

TEST_CASE("AssignmentModel hard constraints", "[assignment]") {
    coso::AssignmentModel m;
    m.set_max_consecutive_shifts(5);
    m.set_min_rest_between_shifts(11);
    int s1 = m.add_shift_type({.name = "Night"});
    int s2 = m.add_shift_type({.name = "Morning"});
    m.add_forbidden_sequence({s1, s2});
}

TEST_CASE("AssignmentModel soft constraints", "[assignment]") {
    coso::AssignmentModel m;
    int s = m.add_shift_type({.name = "Day"});
    int e = m.add_employee({.name = "Bob"});
    m.set_horizon(7);
    m.add_preference(e, 0, s, 10);
    m.add_unavailability(e, 3);
}

TEST_CASE("AssignmentModel warm start and replanning", "[assignment]") {
    coso::AssignmentModel m;
    m.set_published_schedule({{0, 1}, {1, 0}});
    m.set_change_penalty(50);
}

TEST_CASE("AssignmentModel solve returns a Result", "[assignment]") {
    coso::AssignmentModel m;
    m.add_shift_type({.name = "Day"});
    m.add_employee({.name = "Alice"});
    m.set_horizon(7);
    coso::Result r = m.solve(coso::TimeLimit(1.0));
    REQUIRE(r.cost() >= 0.0);
    REQUIRE(r.work_ticks() > 0);
    REQUIRE(r.work_units() > 0.0);
}

// --------------------------------------------------------------------------
//  PackingModel
// --------------------------------------------------------------------------

TEST_CASE("PackingModel can be default-constructed", "[packing]") {
    coso::PackingModel m;
    (void)m;
}

TEST_CASE("PackingModel add_bin_type", "[packing]") {
    coso::PackingModel m;
    int bt = m.add_bin_type({.capacity = {100}, .cost = 1});
    REQUIRE(bt >= 0);
}

TEST_CASE("PackingModel add_item", "[packing]") {
    coso::PackingModel m;
    int it = m.add_item({.size = {25}});
    REQUIRE(it >= 0);
}

TEST_CASE("PackingModel conflicts", "[packing]") {
    coso::PackingModel m;
    int a = m.add_item({.size = {10}});
    int b = m.add_item({.size = {20}});
    m.add_conflict(a, b);
}

TEST_CASE("PackingModel solve returns a Result", "[packing]") {
    coso::PackingModel m;
    m.add_bin_type({.capacity = {100}});
    m.add_item({.size = {30}});
    m.add_item({.size = {40}});
    coso::Result r = m.solve(coso::TimeLimit(1.0));
    REQUIRE(r.cost() >= 0.0);
    REQUIRE(r.work_ticks() > 0);
    REQUIRE(r.work_units() > 0.0);
}

TEST_CASE("Deterministic stop parity across model APIs", "[model][work_units]") {
    SECTION("ScheduleModel") {
        SKIP(
            "ScheduleModel::solve() aborts via construct_neh() on any instance with 2 or "
            "more jobs — coso#188");
        coso::ScheduleModel m;
        m.add_machine();
        int j0 = m.add_job();
        int j1 = m.add_job();
        m.add_operation(j0, {.machine = 0, .duration = 3});
        m.add_operation(j1, {.machine = 0, .duration = 2});

        auto r1 = m.solve(coso::TimeLimit(1.0, 0.05));
        auto r2 = m.solve(coso::TimeLimit(1.0, 0.05));

        REQUIRE(r1.work_ticks() > 0);
        REQUIRE(r1.work_ticks() == r2.work_ticks());
        REQUIRE(r1.work_units() == r2.work_units());
    }

    SECTION("AssignmentModel") {
        coso::AssignmentModel m;
        int day = m.add_shift_type({.name = "Day"});
        m.add_employee({.name = "Alice"});
        m.add_employee({.name = "Bob"});
        m.set_horizon(4);
        m.add_demand(day, {.min_employees = 1, .max_employees = 1});

        auto r1 = m.solve(coso::TimeLimit(1.0, 0.05));
        auto r2 = m.solve(coso::TimeLimit(1.0, 0.05));

        REQUIRE(r1.work_ticks() > 0);
        REQUIRE(r1.work_ticks() == r2.work_ticks());
        REQUIRE(r1.work_units() == r2.work_units());
    }

    SECTION("PackingModel") {
        coso::PackingModel m;
        m.add_bin_type({.capacity = {10}});
        m.add_item({.size = {6}});
        m.add_item({.size = {4}});
        m.add_item({.size = {3}});

        auto r1 = m.solve(coso::TimeLimit(1.0, 0.05));
        auto r2 = m.solve(coso::TimeLimit(1.0, 0.05));

        REQUIRE(r1.work_ticks() > 0);
        REQUIRE(r1.work_ticks() == r2.work_ticks());
        REQUIRE(r1.work_units() == r2.work_units());
    }

    SECTION("NetworkModel") {
        coso::NetworkModel m;
        int s = m.add_node(5, "s");
        int t = m.add_node(-5, "t");
        m.add_arc(s, t, 2, 0, 5);

        auto r1 = m.solve(coso::TimeLimit(1.0, 0.05));
        auto r2 = m.solve(coso::TimeLimit(1.0, 0.05));

        REQUIRE(r1.work_ticks() > 0);
        REQUIRE(r1.work_ticks() == r2.work_ticks());
        REQUIRE(r1.work_units() == r2.work_units());
    }

    SECTION("LotSizingModel") {
        coso::LotSizingModel m;
        m.set_num_periods(3);
        int p = m.add_product(100.0, 2.0, 1.0, 2.0);
        m.set_demand(p, 0, 10.0);
        m.set_demand(p, 1, 15.0);
        m.set_demand(p, 2, 20.0);
        m.set_capacity(0, 80.0);
        m.set_capacity(1, 80.0);
        m.set_capacity(2, 80.0);

        auto r1 = m.solve(coso::TimeLimit(1.0, 0.05));
        auto r2 = m.solve(coso::TimeLimit(1.0, 0.05));

        REQUIRE(r1.work_ticks() > 0);
        REQUIRE(r1.work_ticks() == r2.work_ticks());
        REQUIRE(r1.work_units() == r2.work_units());
    }
}

// ==========================================================================
//  Introspection round-trips (#216)
//
//  What a model was told, read back off the model itself.  Every accessor
//  added by #216 is called at least once here and every field of every
//  exposed entry struct is asserted, so dropping an accessor fails the build
//  and dropping a field from a struct fails to compile the assertion that
//  reads it.
// ==========================================================================

TEST_CASE("RoutingModel reads back every declaration", "[routing][introspection]") {
    coso::RoutingModel m;

    SECTION("depots, clients and vehicle types round-trip field by field") {
        coso::DepotParams dp;
        dp.tw = {5, 500};
        REQUIRE(m.add_depot(1.5, -2.5, dp) == 0);
        REQUIRE(m.add_depot(42, dp) == 1);

        coso::ClientParams cp;
        cp.demand = {3, 4};
        cp.pickup = {1, 2};
        cp.tw = {10, 90};
        cp.extra_tw = {{100, 110}, {120, 130}};
        cp.service = 7;
        cp.release_time = 8;
        cp.prize = 9;
        cp.required = false;
        cp.group = 11;
        cp.skills = {"crane", "fridge"};
        cp.client_type = 2;
        REQUIRE(m.add_client(3.5, 4.5, cp) == 0);
        REQUIRE(m.add_client(77, cp) == 1);

        coso::VehicleTypeParams vp;
        vp.capacity = {50, 60};
        vp.max_duration = 480;
        vp.max_distance = 900;
        vp.min_tasks = 1;
        vp.max_tasks = 12;
        vp.max_overtime = 30;
        vp.unit_overtime_cost = 5;
        vp.reload_depot = 1;
        vp.max_reloads = 2;
        vp.cost.fixed_cost = 100;
        vp.cost.unit_distance_cost = 3;
        vp.cost.unit_duration_cost = 4;
        vp.profile = 1;
        vp.skills = {"crane"};
        REQUIRE(m.add_vehicle_type(6, vp) == 0);

        REQUIRE(m.num_depots() == 2);
        auto const& d0 = m.depot(0);
        REQUIRE(d0.x == 1.5);
        REQUIRE(d0.y == -2.5);
        REQUIRE(d0.has_coord);
        REQUIRE(d0.explicit_id == -1);
        REQUIRE(d0.params.tw.start == 5);
        REQUIRE(d0.params.tw.end == 500);

        // Trap: a depot added by explicit id stores x = y = 0.0, so has_coord
        // is the only thing telling it apart from a depot at the origin.
        auto const& d1 = m.depot(1);
        REQUIRE(d1.x == 0.0);
        REQUIRE(d1.y == 0.0);
        REQUIRE_FALSE(d1.has_coord);
        REQUIRE(d1.explicit_id == 42);

        REQUIRE(m.num_clients() == 2);
        auto const& c0 = m.client(0);
        REQUIRE(c0.x == 3.5);
        REQUIRE(c0.y == 4.5);
        REQUIRE(c0.has_coord);
        REQUIRE(c0.explicit_id == -1);
        REQUIRE(c0.params.demand == std::vector<int>{3, 4});
        REQUIRE(c0.params.pickup == std::vector<int>{1, 2});
        REQUIRE(c0.params.tw.start == 10);
        REQUIRE(c0.params.tw.end == 90);
        REQUIRE(c0.params.extra_tw.size() == 2);
        REQUIRE(c0.params.extra_tw[1].start == 120);
        REQUIRE(c0.params.extra_tw[1].end == 130);
        REQUIRE(c0.params.service == 7);
        REQUIRE(c0.params.release_time == 8);
        REQUIRE(c0.params.prize == 9);
        REQUIRE_FALSE(c0.params.required);
        REQUIRE(c0.params.group == 11);
        REQUIRE(c0.params.skills == std::vector<std::string>{"crane", "fridge"});
        REQUIRE(c0.params.client_type == 2);

        // Trap: same for clients added by explicit id.
        auto const& c1 = m.client(1);
        REQUIRE(c1.x == 0.0);
        REQUIRE(c1.y == 0.0);
        REQUIRE_FALSE(c1.has_coord);
        REQUIRE(c1.explicit_id == 77);

        REQUIRE(m.num_vehicle_types() == 1);
        auto const& v0 = m.vehicle_type(0);
        REQUIRE(v0.count == 6);
        REQUIRE(v0.params.capacity == std::vector<int>{50, 60});
        REQUIRE(v0.params.max_duration == 480);
        REQUIRE(v0.params.max_distance == 900);
        REQUIRE(v0.params.min_tasks == 1);
        REQUIRE(v0.params.max_tasks == 12);
        REQUIRE(v0.params.max_overtime == 30);
        REQUIRE(v0.params.unit_overtime_cost == 5);
        REQUIRE(v0.params.reload_depot == 1);
        REQUIRE(v0.params.max_reloads == 2);
        REQUIRE(v0.params.cost.fixed_cost == 100);
        REQUIRE(v0.params.cost.unit_distance_cost == 3);
        REQUIRE(v0.params.cost.unit_duration_cost == 4);
        REQUIRE(v0.params.profile == 1);
        REQUIRE(v0.params.skills == std::vector<std::string>{"crane"});
    }

    SECTION("add_pickup and add_delivery store plain clients; only the pairing survives") {
        // Trap: add_pickup / add_delivery are literal aliases for add_client.
        // The role is not stored, so the stored entries are indistinguishable
        // and only add_request() records anything about the pair.
        int p = m.add_pickup(1.0, 1.0);
        int d = m.add_delivery(2.0, 2.0);
        int plain = m.add_client(3.0, 3.0);
        m.add_request(p, d);
        m.add_pickup_delivery(d, plain);

        REQUIRE(m.num_clients() == 3);
        REQUIRE(m.client(p).has_coord);
        REQUIRE(m.client(d).has_coord);
        // Nothing on a stored client distinguishes a pickup from a delivery
        // from a plain client: all three carry default ClientParams.
        REQUIRE(m.client(p).params.required);
        REQUIRE(m.client(d).params.required);
        REQUIRE(m.client(plain).params.required);
        REQUIRE(m.client(p).params.demand.empty());
        REQUIRE(m.client(d).params.demand.empty());

        REQUIRE(m.requests() == std::vector<std::pair<int, int>>{{p, d}, {d, plain}});
    }

    SECTION("client groups count the ids handed out, not the ids clients carry") {
        REQUIRE(m.num_client_groups() == 0);
        REQUIRE(m.add_client_group() == 0);
        REQUIRE(m.add_client_group() == 1);
        REQUIRE(m.num_client_groups() == 2);

        // ClientParams::group is never validated against next_group_id_, so a
        // client may name a group that was never created and the count does
        // not bound it.
        coso::ClientParams cp;
        cp.group = 99;
        m.add_client(0.0, 0.0, cp);
        REQUIRE(m.client(0).params.group == 99);
        REQUIRE(m.num_client_groups() == 2);
    }

    SECTION("matrix setters are an append-only log, last entry wins") {
        m.set_distance(0, 1, 10);
        m.set_duration(0, 1, 20);
        m.set_profile(2);
        m.set_distance(0, 1, 30);  // same (from, to), new profile
        m.set_duration(0, 1, 40);
        m.set_profile_distance(3, 1, 0, 50);
        m.set_profile_duration(3, 1, 0, 60);
        m.set_cost_matrix(1, 0, 1, 70);

        REQUIRE(m.distance_entries().size() == 3);
        auto const& e0 = m.distance_entries()[0];
        REQUIRE(e0.profile == 0);
        REQUIRE(e0.from == 0);
        REQUIRE(e0.to == 1);
        REQUIRE(e0.value == 10);
        REQUIRE(m.distance_entries()[1].profile == 2);
        REQUIRE(m.distance_entries()[1].value == 30);
        REQUIRE(m.distance_entries()[2].profile == 3);
        REQUIRE(m.distance_entries()[2].from == 1);
        REQUIRE(m.distance_entries()[2].to == 0);
        REQUIRE(m.distance_entries()[2].value == 50);

        REQUIRE(m.duration_entries().size() == 3);
        REQUIRE(m.duration_entries()[0].value == 20);
        REQUIRE(m.duration_entries()[1].value == 40);
        REQUIRE(m.duration_entries()[2].value == 60);

        REQUIRE(m.cost_entries().size() == 1);
        REQUIRE(m.cost_entries()[0].profile == 1);
        REQUIRE(m.cost_entries()[0].from == 0);
        REQUIRE(m.cost_entries()[0].to == 1);
        REQUIRE(m.cost_entries()[0].value == 70);

        // Trap: a repeated (profile, from, to) appends rather than
        // overwriting, so the log holds duplicates and the reader must take
        // the last one.
        m.set_profile_distance(0, 0, 1, 11);
        REQUIRE(m.distance_entries().size() == 4);
        REQUIRE(m.distance_entries()[0].value == 10);
        REQUIRE(m.distance_entries()[3].profile == 0);
        REQUIRE(m.distance_entries()[3].from == 0);
        REQUIRE(m.distance_entries()[3].to == 1);
        REQUIRE(m.distance_entries()[3].value == 11);

        // The three plain setters append the same way -- asserted separately,
        // because a set_profile_distance duplicate does not exercise them.
        m.set_distance(0, 1, 12);
        REQUIRE(m.distance_entries().size() == 5);
        REQUIRE(m.distance_entries()[0].value == 10);
        REQUIRE(m.distance_entries()[4].value == 12);

        m.set_duration(0, 1, 21);
        REQUIRE(m.duration_entries().size() == 4);
        REQUIRE(m.duration_entries()[0].value == 20);
        REQUIRE(m.duration_entries()[3].value == 21);

        m.set_cost_matrix(1, 0, 1, 71);
        REQUIRE(m.cost_entries().size() == 2);
        REQUIRE(m.cost_entries()[0].value == 70);
        REQUIRE(m.cost_entries()[1].value == 71);
    }

    SECTION("warm start and pins round-trip verbatim") {
        std::vector<std::vector<int>> routes = {{0, 1, 2}, {}, {3}};
        m.set_initial_routes(routes);
        REQUIRE(m.initial_routes() == routes);

        // set_initial_routes assigns, so a second call replaces the first.
        m.set_initial_routes({{4}});
        REQUIRE(m.initial_routes() == std::vector<std::vector<int>>{{4}});

        // Trap: pin() appends with no dedup and no range check, so the same
        // id can appear twice and an id no client owns is stored as given.
        REQUIRE(m.pinned().empty());
        m.pin(2);
        m.pin(2);
        m.pin(9999);
        m.pin(-1);
        REQUIRE(m.pinned() == std::vector<int>{2, 2, 9999, -1});
        REQUIRE(m.num_clients() == 0);
    }
}

TEST_CASE("NetworkModel reads back every declaration", "[network][introspection]") {
    coso::NetworkModel m;

    REQUIRE(m.num_nodes() == 0);
    REQUIRE(m.num_arcs() == 0);

    int s = m.add_node(15, "source");
    int t = m.add_node(-15, "sink");
    int mid = m.add_node();  // defaults: supply 0, empty name
    int a = m.add_arc(s, mid, 7, 2, 20);
    int b = m.add_arc(mid, t);  // defaults: cost 0, lower 0, upper INT_MAX

    REQUIRE(m.num_nodes() == 3);
    REQUIRE(m.node(s).supply == 15);
    REQUIRE(m.node(s).name == "source");
    REQUIRE(m.node(t).supply == -15);
    REQUIRE(m.node(t).name == "sink");
    REQUIRE(m.node(mid).supply == 0);
    REQUIRE(m.node(mid).name.empty());

    REQUIRE(m.num_arcs() == 2);
    REQUIRE(m.arc(a).tail == s);
    REQUIRE(m.arc(a).head == mid);
    REQUIRE(m.arc(a).cost == 7);
    REQUIRE(m.arc(a).lower_cap == 2);
    REQUIRE(m.arc(a).upper_cap == 20);
    REQUIRE(m.arc(b).tail == mid);
    REQUIRE(m.arc(b).head == t);
    REQUIRE(m.arc(b).cost == 0);
    REQUIRE(m.arc(b).lower_cap == 0);
    REQUIRE(m.arc(b).upper_cap == INT_MAX);
}

TEST_CASE("LotSizingModel reads back every declaration", "[lotsizing][introspection]") {
    SECTION("products, demand, capacity and BOM round-trip") {
        coso::LotSizingModel m;
        REQUIRE(m.num_periods() == 0);
        REQUIRE(m.num_products() == 0);

        m.set_num_periods(3);
        REQUIRE(m.num_periods() == 3);
        REQUIRE(m.capacities() == std::vector<double>{0.0, 0.0, 0.0});

        int p0 = m.add_product(100.0, 2.0, 1.5, 0.25);
        int p1 = m.add_product(50.0, 1.0, 0.5, 0.75);
        REQUIRE(m.num_products() == 2);
        REQUIRE(m.product(p0).setup_cost == 100.0);
        REQUIRE(m.product(p0).setup_time == 2.0);
        REQUIRE(m.product(p0).unit_production_cost == 1.5);
        REQUIRE(m.product(p0).holding_cost == 0.25);
        REQUIRE(m.product(p1).setup_cost == 50.0);
        REQUIRE(m.product(p1).setup_time == 1.0);
        REQUIRE(m.product(p1).unit_production_cost == 0.5);
        REQUIRE(m.product(p1).holding_cost == 0.75);

        m.set_demand(p0, 0, 10.0);
        m.set_demand(p0, 2, 30.0);
        m.set_demand(p1, 1, 5.0);
        m.set_capacity(0, 80.0);
        m.set_capacity(2, 90.0);

        REQUIRE(m.demands().size() == 2);
        REQUIRE(m.demands()[p0] == std::vector<double>{10.0, 0.0, 30.0});
        REQUIRE(m.demands()[p1] == std::vector<double>{0.0, 5.0, 0.0});
        REQUIRE(m.capacities() == std::vector<double>{80.0, 0.0, 90.0});

        m.add_bom(p0, p1, 2.5);
        REQUIRE(m.bom().size() == 1);
        REQUIRE(m.bom()[0].parent == p0);
        REQUIRE(m.bom()[0].child == p1);
        REQUIRE(m.bom()[0].quantity == 2.5);
    }

    SECTION("set_num_periods wipes demand and capacity, so call order is load-bearing") {
        // Trap: set_num_periods() re-assigns demands_ and capacities_.
        coso::LotSizingModel m;
        m.set_num_periods(2);
        int p = m.add_product(1.0, 0.0, 1.0, 1.0);
        m.set_demand(p, 0, 42.0);
        m.set_capacity(1, 99.0);
        REQUIRE(m.demands()[p][0] == 42.0);
        REQUIRE(m.capacities()[1] == 99.0);

        m.set_num_periods(4);
        REQUIRE(m.num_periods() == 4);
        REQUIRE(m.num_products() == 1);  // products survive
        REQUIRE(m.demands()[p] == std::vector<double>{0.0, 0.0, 0.0, 0.0});
        REQUIRE(m.capacities() == std::vector<double>{0.0, 0.0, 0.0, 0.0});
    }

    SECTION("a product added before set_num_periods has an empty demand row") {
        // Trap: add_product with num_periods_ == 0 pushes an empty row, so
        // demands() is ragged until set_num_periods() is called at all.
        coso::LotSizingModel m;
        int p = m.add_product(1.0, 0.0, 1.0, 1.0);
        REQUIRE(m.num_products() == 1);
        REQUIRE(m.num_periods() == 0);
        REQUIRE(m.demands().size() == 1);
        REQUIRE(m.demands()[p].empty());
        REQUIRE(m.capacities().empty());
        REQUIRE(m.bom().empty());
    }
}

TEST_CASE("ScheduleModel reads back every declaration", "[scheduling][introspection]") {
    coso::ScheduleModel m;

    SECTION("machines, jobs, operations and precedences round-trip") {
        REQUIRE(m.num_machines() == 0);
        REQUIRE(m.num_jobs() == 0);
        REQUIRE(m.num_operations() == 0);
        REQUIRE(m.objective() == coso::ScheduleObjective::Makespan);

        int m0 = m.add_machine({.name = "drill"});
        int m1 = m.add_machine();
        REQUIRE(m.num_machines() == 2);
        REQUIRE(m.machine(m0).name == "drill");
        REQUIRE(m.machine(m1).name.empty());

        coso::JobParams jp;
        jp.name = "widget";
        jp.release_time = 3;
        jp.due_date = 100;
        jp.weight = 4;
        int j0 = m.add_job(jp);
        int j1 = m.add_job();
        REQUIRE(m.num_jobs() == 2);
        REQUIRE(m.job(j0).name == "widget");
        REQUIRE(m.job(j0).release_time == 3);
        REQUIRE(m.job(j0).due_date == 100);
        REQUIRE(m.job(j0).weight == 4);
        REQUIRE(m.job(j1).name.empty());
        REQUIRE(m.job(j1).release_time == 0);
        REQUIRE(m.job(j1).due_date == INT_MAX);
        REQUIRE(m.job(j1).weight == 1);

        coso::OperationParams fixed;
        fixed.machine = m0;
        fixed.duration = 12;
        int o0 = m.add_operation(j0, fixed);

        coso::OperationParams flexible;
        flexible.eligible_machines = {m0, m1};
        flexible.durations_per_machine = {7, 9};
        flexible.optional = true;
        int o1 = m.add_operation(j0, flexible);
        int o2 = m.add_operation(j1, fixed);

        REQUIRE(m.num_operations() == 3);
        REQUIRE(m.operation(o0).job == j0);
        REQUIRE(m.operation(o0).params.machine == m0);
        REQUIRE(m.operation(o0).params.duration == 12);
        REQUIRE(m.operation(o0).params.eligible_machines.empty());
        REQUIRE(m.operation(o0).params.durations_per_machine.empty());
        REQUIRE_FALSE(m.operation(o0).params.optional);
        REQUIRE(m.operation(o1).job == j0);
        REQUIRE(m.operation(o1).params.machine == -1);
        REQUIRE(m.operation(o1).params.eligible_machines == std::vector<int>{m0, m1});
        REQUIRE(m.operation(o1).params.durations_per_machine == std::vector<int>{7, 9});
        REQUIRE(m.operation(o1).params.optional);

        REQUIRE(m.job_operations() == std::vector<std::vector<int>>{{o0, o1}, {o2}});

        m.add_precedence(o0, o2);
        m.add_precedence(o2, o1);
        REQUIRE(m.extra_precedences().size() == 2);
        REQUIRE(m.extra_precedences()[0].before == o0);
        REQUIRE(m.extra_precedences()[0].after == o2);
        REQUIRE(m.extra_precedences()[1].before == o2);
        REQUIRE(m.extra_precedences()[1].after == o1);
    }

    SECTION("resource usage comes back ragged, unset meaning zero") {
        m.add_machine();
        int j = m.add_job();
        int o0 = m.add_operation(j, {.machine = 0, .duration = 5});
        int o1 = m.add_operation(j, {.machine = 0, .duration = 5});
        int o2 = m.add_operation(j, {.machine = 0, .duration = 5});

        int r0 = m.add_resource(10);
        int r1 = m.add_resource(20);
        int r2 = m.add_resource(30);
        REQUIRE(m.num_resources() == 3);
        REQUIRE(m.resource_capacities() == std::vector<int>{10, 20, 30});

        m.set_resource_usage(o0, r0, 4);
        m.set_resource_usage(o1, r2, 6);

        // Trap: rows are grown only to resource + 1, so they are ragged and
        // nothing is padded out to num_resources().
        REQUIRE(m.resource_usage().size() == 3);
        REQUIRE(m.resource_usage()[o0] == std::vector<int>{4});
        REQUIRE(m.resource_usage()[o1] == std::vector<int>{0, 0, 6});
        REQUIRE(m.resource_usage()[o2].empty());
        // Which means a reader must treat a short row as unset, i.e. 0.
        REQUIRE(static_cast<int>(m.resource_usage()[o0].size()) < m.num_resources());
        REQUIRE(r1 == 1);
    }

    SECTION("objective and initial schedule round-trip") {
        m.set_objective(coso::ScheduleObjective::TotalWeightedTardiness);
        REQUIRE(m.objective() == coso::ScheduleObjective::TotalWeightedTardiness);
        m.set_objective(coso::ScheduleObjective::TotalFlowTime);
        REQUIRE(m.objective() == coso::ScheduleObjective::TotalFlowTime);
        m.minimize_makespan();
        REQUIRE(m.objective() == coso::ScheduleObjective::Makespan);

        REQUIRE(m.initial_schedule().empty());
        std::vector<std::pair<int, int>> assignments = {{0, 0}, {1, 12}};
        m.set_initial_schedule(assignments);
        REQUIRE(m.initial_schedule() == assignments);
    }
}
