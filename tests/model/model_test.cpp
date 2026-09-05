#include <catch2/catch_test_macros.hpp>

#include <model/assignment_model.h>
#include <model/lotsizing_model.h>
#include <model/network_model.h>
#include <model/packing_model.h>
#include <model/routing_model.h>
#include <model/schedule_model.h>

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
    int p = m.add_pickup(1.0, 0.0, {.quantity = 3});
    int d = m.add_delivery(2.0, 0.0, {.quantity = 3});
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

TEST_CASE("PackingModel minimize_bins", "[packing]") {
    coso::PackingModel m;
    m.minimize_bins();
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
