#include <catch2/catch_test_macros.hpp>

#include <climits>
#include <model/lotsizing_model.h>
#include <model/network_model.h>
#include <model/packing_model.h>
#include <model/rostering_model.h>
#include <model/routing_model.h>
#include <model/schedule_model.h>
#include <stdexcept>
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
    REQUIRE(r.roster().empty());
    REQUIRE(r.unassigned().empty());
    REQUIRE(r.bins().empty());
    REQUIRE(r.num_bins() == 0);
    REQUIRE(r.flows().empty());
}

TEST_CASE("TimeLimit stores seconds", "[types]") {
    coso::TimeLimit tl(30.0);
    REQUIRE(tl.seconds == 30.0);
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

TEST_CASE("RoutingModel pickup-delivery workflow", "[routing]") {
    coso::RoutingModel m;
    m.add_depot(0.0, 0.0);
    int p = m.add_pickup(1.0, 0.0, {.pickup = {3}});
    int d = m.add_delivery(2.0, 0.0, {.demand = {3}});
    m.add_request(p, d);
    // add_pickup_delivery is an alias
    m.add_pickup_delivery(p, d);
}

TEST_CASE("RoutingModel distance and duration setters", "[routing]") {
    coso::RoutingModel m;
    m.set_distance(0, 1, 100);
    m.set_duration(0, 1, 50);
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

    int a0 = m.add_arc(src, mid, 1, 5);
    int a1 = m.add_arc(mid, dst, 1, 5);
    REQUIRE(a0 == 0);
    REQUIRE(a1 == 1);
}

TEST_CASE("NetworkModel solve returns flow result", "[network]") {
    coso::NetworkModel m;
    int src = m.add_node(5, "src");
    int dst = m.add_node(-5, "dst");
    m.add_arc(src, dst, 2, 5);

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
    int mach = m.add_machine({});
    REQUIRE(mach >= 0);
}

TEST_CASE("ScheduleModel add_job and add_operation", "[scheduling]") {
    coso::ScheduleModel m;
    int j = m.add_job({.weight = 2});
    REQUIRE(j >= 0);
    int op = m.add_operation(j, {.machine = 0, .duration = 10});
    REQUIRE(op >= 0);
}

TEST_CASE("ScheduleModel FJSP flexible operations", "[scheduling]") {
    coso::ScheduleModel m;
    m.add_machine({});
    m.add_machine({});
    int j = m.add_job();
    int op = m.add_operation(j, {
                                    .eligible_machines = {0, 1},
                                    .durations_per_machine = {5, 8},
                                });
    REQUIRE(op >= 0);
}

TEST_CASE("ScheduleModel objectives", "[scheduling]") {
    coso::ScheduleModel m;
    m.set_objective(coso::ScheduleObjective::Makespan);
    m.set_objective(coso::ScheduleObjective::TotalWeightedTardiness);
    m.minimize_makespan();
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
//  RosteringModel
// --------------------------------------------------------------------------

TEST_CASE("RosteringModel can be default-constructed", "[rostering]") {
    coso::RosteringModel m;
    (void)m;
}

TEST_CASE("RosteringModel add_shift_type and add_employee", "[rostering]") {
    coso::RosteringModel m;
    int s = m.add_shift_type({.name = "Morning", .duration_minutes = 480});
    REQUIRE(s >= 0);
    int e = m.add_employee({.name = "Alice", .skills = {"ICU"}});
    REQUIRE(e >= 0);
}

TEST_CASE("RosteringModel planning horizon and demand", "[rostering]") {
    coso::RosteringModel m;
    int s = m.add_shift_type({.name = "Day"});
    m.set_horizon(7);
    m.add_demand(s, 0, {.min_employees = 2});
}

TEST_CASE("RosteringModel hard constraints", "[rostering]") {
    coso::RosteringModel m;
    int s1 = m.add_shift_type({.name = "Night"});
    int s2 = m.add_shift_type({.name = "Morning"});
    m.add_forbidden_sequence(s1, s2);
}

TEST_CASE("RosteringModel soft constraints", "[rostering]") {
    coso::RosteringModel m;
    int s = m.add_shift_type({.name = "Day"});
    int e = m.add_employee({.name = "Bob"});
    m.set_horizon(7);
    m.add_preference(e, 0, s, 10);
    m.add_unavailability(e, 3);
}

TEST_CASE("RosteringModel solve returns a Result", "[rostering]") {
    coso::RosteringModel m;
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

TEST_CASE("PackingModel set_bin_capacity", "[packing]") {
    coso::PackingModel m;
    m.set_bin_capacity({100});
    REQUIRE(m.bin_capacity() == std::vector<int>{100});
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
    m.set_bin_capacity({100});
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

    SECTION("RosteringModel") {
        coso::RosteringModel m;
        int day = m.add_shift_type({.name = "Day"});
        m.add_employee({.name = "Alice"});
        m.add_employee({.name = "Bob"});
        m.set_horizon(4);
        for (int d = 0; d < 4; ++d) {
            m.add_demand(day, d, {.min_employees = 1, .max_employees = 1});
        }

        auto r1 = m.solve(coso::TimeLimit(1.0, 0.05));
        auto r2 = m.solve(coso::TimeLimit(1.0, 0.05));

        REQUIRE(r1.work_ticks() > 0);
        REQUIRE(r1.work_ticks() == r2.work_ticks());
        REQUIRE(r1.work_units() == r2.work_units());
    }

    SECTION("PackingModel") {
        coso::PackingModel m;
        m.set_bin_capacity({10});
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
        m.add_arc(s, t, 2, 5);

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

        coso::ClientParams cp;
        cp.demand = {3, 4};
        cp.pickup = {1, 2};
        cp.tw = {10, 90};
        cp.service = 7;
        cp.prize = 9;
        cp.required = false;
        REQUIRE(m.add_client(3.5, 4.5, cp) == 0);

        coso::VehicleTypeParams vp;
        vp.capacity = {50, 60};
        vp.max_duration = 480;
        vp.max_distance = 900;
        REQUIRE(m.add_vehicle_type(6, vp) == 0);

        REQUIRE(m.num_depots() == 1);
        auto const& d0 = m.depot(0);
        REQUIRE(d0.x == 1.5);
        REQUIRE(d0.y == -2.5);
        REQUIRE(d0.params.tw.start == 5);
        REQUIRE(d0.params.tw.end == 500);

        REQUIRE(m.num_clients() == 1);
        auto const& c0 = m.client(0);
        REQUIRE(c0.x == 3.5);
        REQUIRE(c0.y == 4.5);
        REQUIRE(c0.params.demand == std::vector<int>{3, 4});
        REQUIRE(c0.params.pickup == std::vector<int>{1, 2});
        REQUIRE(c0.params.tw.start == 10);
        REQUIRE(c0.params.tw.end == 90);
        REQUIRE(c0.params.service == 7);
        REQUIRE(c0.params.prize == 9);
        REQUIRE_FALSE(c0.params.required);

        REQUIRE(m.num_vehicle_types() == 1);
        auto const& v0 = m.vehicle_type(0);
        REQUIRE(v0.count == 6);
        REQUIRE(v0.params.capacity == std::vector<int>{50, 60});
        REQUIRE(v0.params.max_duration == 480);
        REQUIRE(v0.params.max_distance == 900);
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
        // Nothing on a stored client distinguishes a pickup from a delivery
        // from a plain client: all three carry default ClientParams.
        REQUIRE(m.client(p).params.required);
        REQUIRE(m.client(d).params.required);
        REQUIRE(m.client(plain).params.required);
        REQUIRE(m.client(p).params.demand.empty());
        REQUIRE(m.client(d).params.demand.empty());

        REQUIRE(m.requests() == std::vector<std::pair<int, int>>{{p, d}, {d, plain}});
    }

    SECTION("matrix setters are an append-only log, last entry wins") {
        m.set_distance(0, 1, 10);
        m.set_duration(0, 1, 20);
        m.set_distance(1, 0, 50);
        m.set_duration(1, 0, 60);

        REQUIRE(m.distance_entries().size() == 2);
        auto const& e0 = m.distance_entries()[0];
        REQUIRE(e0.from == 0);
        REQUIRE(e0.to == 1);
        REQUIRE(e0.value == 10);
        REQUIRE(m.distance_entries()[1].from == 1);
        REQUIRE(m.distance_entries()[1].to == 0);
        REQUIRE(m.distance_entries()[1].value == 50);

        REQUIRE(m.duration_entries().size() == 2);
        REQUIRE(m.duration_entries()[0].value == 20);
        REQUIRE(m.duration_entries()[1].value == 60);

        // Trap: a repeated (from, to) appends rather than overwriting, so the
        // log holds duplicates and the reader must take the last one.
        m.set_distance(0, 1, 12);
        REQUIRE(m.distance_entries().size() == 3);
        REQUIRE(m.distance_entries()[0].value == 10);
        REQUIRE(m.distance_entries()[2].from == 0);
        REQUIRE(m.distance_entries()[2].to == 1);
        REQUIRE(m.distance_entries()[2].value == 12);

        m.set_duration(0, 1, 21);
        REQUIRE(m.duration_entries().size() == 3);
        REQUIRE(m.duration_entries()[0].value == 20);
        REQUIRE(m.duration_entries()[2].value == 21);
    }
}

TEST_CASE("NetworkModel reads back every declaration", "[network][introspection]") {
    coso::NetworkModel m;

    REQUIRE(m.num_nodes() == 0);
    REQUIRE(m.num_arcs() == 0);

    int s = m.add_node(15, "source");
    int t = m.add_node(-15, "sink");
    int mid = m.add_node();  // defaults: supply 0, empty name
    int a = m.add_arc(s, mid, 7, 20);
    int b = m.add_arc(mid, t);  // defaults: cost 0, upper INT_MAX

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
    REQUIRE(m.arc(a).upper_cap == 20);
    REQUIRE(m.arc(b).tail == mid);
    REQUIRE(m.arc(b).head == t);
    REQUIRE(m.arc(b).cost == 0);
    REQUIRE(m.arc(b).upper_cap == INT_MAX);
}

TEST_CASE("LotSizingModel reads back every declaration", "[lotsizing][introspection]") {
    SECTION("products, demand and capacity round-trip") {
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
    }
}

TEST_CASE("ScheduleModel reads back every declaration", "[scheduling][introspection]") {
    coso::ScheduleModel m;

    SECTION("machines, jobs and operations round-trip") {
        REQUIRE(m.num_machines() == 0);
        REQUIRE(m.num_jobs() == 0);
        REQUIRE(m.num_operations() == 0);
        REQUIRE(m.objective() == coso::ScheduleObjective::Makespan);

        int m0 = m.add_machine({});
        int m1 = m.add_machine();
        REQUIRE(m.num_machines() == 2);

        coso::JobParams jp;
        jp.due_date = 100;
        jp.weight = 4;
        int j0 = m.add_job(jp);
        int j1 = m.add_job();
        REQUIRE(m.num_jobs() == 2);
        REQUIRE(m.job(j0).due_date == 100);
        REQUIRE(m.job(j0).weight == 4);
        REQUIRE(m.job(j1).due_date == INT_MAX);
        REQUIRE(m.job(j1).weight == 1);

        coso::OperationParams fixed;
        fixed.machine = m0;
        fixed.duration = 12;
        int o0 = m.add_operation(j0, fixed);

        coso::OperationParams flexible;
        flexible.eligible_machines = {m0, m1};
        flexible.durations_per_machine = {7, 9};
        int o1 = m.add_operation(j0, flexible);
        int o2 = m.add_operation(j1, fixed);

        REQUIRE(m.num_operations() == 3);
        REQUIRE(m.operation(o0).job == j0);
        REQUIRE(m.operation(o0).params.machine == m0);
        REQUIRE(m.operation(o0).params.duration == 12);
        REQUIRE(m.operation(o0).params.eligible_machines.empty());
        REQUIRE(m.operation(o0).params.durations_per_machine.empty());
        REQUIRE(m.operation(o1).job == j0);
        REQUIRE(m.operation(o1).params.machine == -1);
        REQUIRE(m.operation(o1).params.eligible_machines == std::vector<int>{m0, m1});
        REQUIRE(m.operation(o1).params.durations_per_machine == std::vector<int>{7, 9});

        REQUIRE(m.job_operations() == std::vector<std::vector<int>>{{o0, o1}, {o2}});
    }

    SECTION("objective round-trips") {
        m.set_objective(coso::ScheduleObjective::TotalWeightedTardiness);
        REQUIRE(m.objective() == coso::ScheduleObjective::TotalWeightedTardiness);
        m.minimize_makespan();
        REQUIRE(m.objective() == coso::ScheduleObjective::Makespan);
    }
}

TEST_CASE("RosteringModel reads back every declaration", "[rostering][introspection]") {
    coso::RosteringModel m;

    SECTION("a fresh model reads back empty defaults") {
        REQUIRE(m.num_shift_types() == 0);
        REQUIRE(m.num_employees() == 0);
        REQUIRE(m.horizon() == 0);
        REQUIRE(m.demands().empty());
        REQUIRE(m.forbidden_sequences().empty());
        REQUIRE(m.preferences().empty());
        REQUIRE(m.unavailabilities().empty());
    }

    SECTION("shift types, employees and the horizon round-trip") {
        int night = m.add_shift_type({.name = "night", .duration_minutes = 480});
        int early = m.add_shift_type({.name = "early", .duration_minutes = 420});
        REQUIRE(m.num_shift_types() == 2);
        REQUIRE(m.shift_type(night).name == "night");
        REQUIRE(m.shift_type(night).duration_minutes == 480);
        REQUIRE(m.shift_type(early).name == "early");
        REQUIRE(m.shift_type(early).duration_minutes == 420);

        int ana =
            m.add_employee({.name = "ana", .skills = {"icu", "triage"}, .max_consecutive_days = 3});
        int bo = m.add_employee({.name = "bo", .skills = {"triage"}, .max_consecutive_days = 2});
        REQUIRE(m.num_employees() == 2);
        REQUIRE(m.employee(ana).name == "ana");
        REQUIRE(m.employee(ana).skills == std::vector<std::string>{"icu", "triage"});
        REQUIRE(m.employee(ana).max_consecutive_days == 3);
        REQUIRE(m.employee(bo).name == "bo");
        REQUIRE(m.employee(bo).skills == std::vector<std::string>{"triage"});
        REQUIRE(m.employee(bo).max_consecutive_days == 2);

        m.set_horizon(14);
        REQUIRE(m.horizon() == 14);
    }

    SECTION("per-day demands round-trip in declaration order") {
        int night = m.add_shift_type({.name = "night"});
        int early = m.add_shift_type({.name = "early"});
        m.set_horizon(7);

        m.add_demand(night, 3, {.min_employees = 2, .max_employees = 5, .required_skill = "icu"});
        m.add_demand(early, 0,
                     {.min_employees = 1, .max_employees = 4, .required_skill = "triage"});

        REQUIRE(m.demands().size() == 2);
        REQUIRE(m.demands()[0].shift_type == night);
        REQUIRE(m.demands()[0].day == 3);
        REQUIRE(m.demands()[0].params.min_employees == 2);
        REQUIRE(m.demands()[0].params.max_employees == 5);
        REQUIRE(m.demands()[0].params.required_skill == "icu");

        REQUIRE(m.demands()[1].shift_type == early);
        REQUIRE(m.demands()[1].day == 0);
        REQUIRE(m.demands()[1].params.min_employees == 1);
        REQUIRE(m.demands()[1].params.max_employees == 4);
        REQUIRE(m.demands()[1].params.required_skill == "triage");
    }

    SECTION("hard constraints, preferences and unavailabilities keep declaration order") {
        m.add_forbidden_sequence(1, 0);
        m.add_forbidden_sequence(0, 1);
        m.add_forbidden_sequence(1, 0);  // no dedup
        REQUIRE(m.forbidden_sequences() ==
                std::vector<std::pair<int, int>>{{1, 0}, {0, 1}, {1, 0}});

        m.add_preference(1, 2, 0, -3);
        m.add_preference(0, 5, 1, 7);
        REQUIRE(m.preferences().size() == 2);
        REQUIRE(m.preferences()[0].employee == 1);
        REQUIRE(m.preferences()[0].day == 2);
        REQUIRE(m.preferences()[0].shift_type == 0);
        REQUIRE(m.preferences()[0].weight == -3);
        REQUIRE(m.preferences()[1].employee == 0);
        REQUIRE(m.preferences()[1].day == 5);
        REQUIRE(m.preferences()[1].shift_type == 1);
        REQUIRE(m.preferences()[1].weight == 7);

        m.add_unavailability(1, 4);
        m.add_unavailability(0, 6);
        REQUIRE(m.unavailabilities().size() == 2);
        REQUIRE(m.unavailabilities()[0].employee == 1);
        REQUIRE(m.unavailabilities()[0].day == 4);
        REQUIRE(m.unavailabilities()[1].employee == 0);
        REQUIRE(m.unavailabilities()[1].day == 6);
    }

    SECTION("element accessors bounds-check both ends") {
        m.add_shift_type({.name = "night"});
        m.add_employee({.name = "ana"});
        REQUIRE_THROWS_AS(m.shift_type(1), std::out_of_range);
        REQUIRE_THROWS_AS(m.shift_type(-1), std::out_of_range);
        REQUIRE_THROWS_AS(m.employee(1), std::out_of_range);
        REQUIRE_THROWS_AS(m.employee(-1), std::out_of_range);
    }
}
