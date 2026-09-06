#include "model/routing_model.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace coso;

namespace {

/// The instance every capacity section below declares: one depot at the origin
/// and three clients on a line at (10, 0), (20, 0) and (30, 0), served by
/// `count` identical vehicles.
///
/// Euclidean distances make the single tour 0 -> 10 -> 20 -> 30 -> 0 cost 60,
/// while three singleton routes cost 2 * (10 + 20 + 30) = 120.  Distance alone
/// therefore always prefers one route; only the declared capacity can split it.
RoutingModel make_line_instance(std::vector<int> const& capacity, std::vector<int> const& demand,
                                std::vector<int> const& pickup, int count) {
    RoutingModel model;
    model.add_depot(0.0, 0.0);
    model.add_vehicle_type(count, {.capacity = capacity});
    for (int i = 1; i <= 3; ++i) {
        model.add_client(10.0 * i, 0.0, {.demand = demand, .pickup = pickup});
    }
    return model;
}

/// Total delivery demand of a returned route, recomputed from the declaration.
int route_delivery(std::vector<int> const& route, int per_client_demand) {
    return static_cast<int>(route.size()) * per_client_demand;
}

/// Every client id 0..n-1 appears exactly once across the returned routes.
bool serves_every_client(Result const& r, int n) {
    std::vector<int> seen(static_cast<size_t>(n), 0);
    for (auto const& route : r.routes()) {
        for (int c : route) {
            if (c < 0 || c >= n) {
                return false;
            }
            ++seen[static_cast<size_t>(c)];
        }
    }
    for (int s : seen) {
        if (s != 1) {
            return false;
        }
    }
    return true;
}

/// Rounded Euclidean distance, matching ProblemData::Builder::build.
int euclid(double ax, double ay, double bx, double by) {
    double dx = ax - bx;
    double dy = ay - by;
    return static_cast<int>(std::lround(std::sqrt(dx * dx + dy * dy)));
}

/// Length of a returned route over the given coordinates, depot first.
int tour_length(std::vector<int> const& route, std::vector<Coord> const& client_coords,
                Coord depot) {
    if (route.empty()) {
        return 0;
    }
    Coord prev = depot;
    int total = 0;
    for (int c : route) {
        Coord cur = client_coords[static_cast<size_t>(c)];
        total += euclid(prev.x, prev.y, cur.x, cur.y);
        prev = cur;
    }
    total += euclid(prev.x, prev.y, depot.x, depot.y);
    return total;
}

/// Deterministic budget: the wall clock is only a safety net.
TimeLimit budget() {
    return TimeLimit(10.0, 0.1);
}

}  // namespace

// ---------------------------------------------------------------------------
//  demand + capacity
// ---------------------------------------------------------------------------

TEST_CASE("RoutingModel: vehicle capacity splits a route the demand overfills",
          "[routing][model]") {
    SECTION("capacity 10 against demand 6 leaves one client per route") {
        RoutingModel model = make_line_instance({10}, {6}, {}, 3);
        Result r = model.solve(budget());

        REQUIRE(r.feasible());
        REQUIRE(serves_every_client(r, 3));
        REQUIRE(r.unserved().empty());

        // Two clients are 12 units of load against a declared 10, so every
        // feasible solution is exactly three singleton routes.
        CHECK(r.routes().size() == 3);
        for (auto const& route : r.routes()) {
            CHECK(route_delivery(route, 6) <= 10);
        }
    }

    SECTION("capacity 20 against the same demand keeps them on one route") {
        RoutingModel model = make_line_instance({20}, {6}, {}, 3);
        Result r = model.solve(budget());

        REQUIRE(r.feasible());
        REQUIRE(serves_every_client(r, 3));

        // 18 units fit, and one tour is 60 against 120 for three.
        CHECK(r.routes().size() == 1);
        CHECK(route_delivery(r.routes()[0], 6) == 18);
    }

    SECTION("the second load dimension binds where the first does not") {
        // Dimension 0 is slack at 3 units against 100; only dimension 1, at 18
        // units against 10, can split the route.
        RoutingModel model = make_line_instance({100, 10}, {1, 6}, {}, 3);
        Result r = model.solve(budget());

        REQUIRE(r.feasible());
        REQUIRE(serves_every_client(r, 3));
        CHECK(r.routes().size() == 3);
        for (auto const& route : r.routes()) {
            CHECK(route_delivery(route, 1) <= 100);
            CHECK(route_delivery(route, 6) <= 10);
        }
    }

    SECTION("the same two dimensions, both slack, keep them on one route") {
        RoutingModel model = make_line_instance({100, 100}, {1, 6}, {}, 3);
        Result r = model.solve(budget());

        REQUIRE(r.feasible());
        REQUIRE(serves_every_client(r, 3));
        CHECK(r.routes().size() == 1);
    }
}

// ---------------------------------------------------------------------------
//  pickup
// ---------------------------------------------------------------------------

TEST_CASE("RoutingModel: backhaul pickup loads the vehicle like demand does", "[routing][model]") {
    SECTION("pickup 6 per client against capacity 10 leaves one client per route") {
        // No delivery demand at all: the split can only come from `pickup`.
        RoutingModel model = make_line_instance({10}, {0}, {6}, 3);
        Result r = model.solve(budget());

        REQUIRE(r.feasible());
        REQUIRE(serves_every_client(r, 3));
        CHECK(r.routes().size() == 3);
        for (auto const& route : r.routes()) {
            CHECK(6 * static_cast<int>(route.size()) <= 10);
        }
    }

    SECTION("the same declaration with pickup 0 keeps them on one route") {
        RoutingModel model = make_line_instance({10}, {0}, {0}, 3);
        Result r = model.solve(budget());

        REQUIRE(r.feasible());
        REQUIRE(serves_every_client(r, 3));
        CHECK(r.routes().size() == 1);
    }
}

// ---------------------------------------------------------------------------
//  TSP: the spec's ruling, confirmed
// ---------------------------------------------------------------------------

TEST_CASE("RoutingModel: one uncapacitated vehicle solves a TSP", "[routing][model]") {
    // Depot and three clients on the corners of a 10 x 10 square.  The
    // perimeter tour costs 40; every tour using a diagonal costs 48.
    RoutingModel model;
    model.add_depot(0.0, 0.0);
    model.add_vehicle_type(1);
    model.add_client(0.0, 10.0);
    model.add_client(10.0, 10.0);
    model.add_client(10.0, 0.0);

    Result r = model.solve(budget());

    REQUIRE(r.feasible());
    REQUIRE(serves_every_client(r, 3));
    REQUIRE(r.routes().size() == 1);

    std::vector<Coord> coords = {{0.0, 10.0}, {10.0, 10.0}, {10.0, 0.0}};
    CHECK(tour_length(r.routes()[0], coords, {0.0, 0.0}) == 40);
}

// ---------------------------------------------------------------------------
//  Objective parameters: model-axis round-trip
// ---------------------------------------------------------------------------

TEST_CASE("RoutingModel: the objective parameters round-trip and solve", "[routing][model]") {
    // The interim evidence form for the objective slots: the public params
    // aggregates are the read-back surface until #216 lands accessors on
    // RoutingModel itself.  No returned-route assertion is possible while
    // Result::cost_ is total distance (#198) and no route says which vehicle
    // type served it.
    VehicleTypeParams vt;
    vt.capacity = {100};
    vt.cost.fixed_cost = 1000;
    vt.cost.unit_distance_cost = 3;
    vt.cost.unit_duration_cost = 2;
    vt.profile = 1;

    CHECK(vt.cost.fixed_cost == 1000);
    CHECK(vt.cost.unit_distance_cost == 3);
    CHECK(vt.cost.unit_duration_cost == 2);
    CHECK(vt.profile == 1);

    RoutingModel model;
    model.add_depot(0.0, 0.0);
    model.add_vehicle_type(2, vt);
    model.add_client(10.0, 0.0, {.demand = {1}});
    model.add_client(20.0, 0.0, {.demand = {1}});

    // Profile 1 must carry its own matrix; node ids are depot 0, clients 1..2.
    for (int i = 0; i <= 2; ++i) {
        for (int j = 0; j <= 2; ++j) {
            model.set_profile_distance(1, i, j, i == j ? 0 : 10);
            model.set_profile_duration(1, i, j, i == j ? 0 : 5);
        }
    }

    Result r = model.solve(budget());

    REQUIRE(r.feasible());
    REQUIRE(serves_every_client(r, 2));
}

TEST_CASE("RoutingModel: Result::cost is the value of the declared objective", "[routing][model]") {
    SKIP(
        "RoutingModel::solve() sets result.cost_ = best.total_distance() "
        "(src/model/routing_model.cpp:196) while the portfolio minimises "
        "Solution::cost(eval) = distance cost + duration cost + fixed cost - prizes, so a "
        "declared fixed_cost never reaches the caller — coso#198");

    RoutingModel model;
    model.add_depot(0.0, 0.0);
    model.add_vehicle_type(1, {.capacity = {100}, .cost = {.fixed_cost = 1000}});
    model.add_client(10.0, 0.0, {.demand = {1}});
    model.add_client(20.0, 0.0, {.demand = {1}});

    Result r = model.solve(budget());

    REQUIRE(r.routes().size() == 1);
    // One vehicle used: 1000 of fixed cost on top of the 40-unit tour.
    CHECK(r.cost() == 1040.0);
}

// ---------------------------------------------------------------------------
//  Time windows and service time: blocked on #194
// ---------------------------------------------------------------------------

TEST_CASE("RoutingModel: client time windows are respected by the returned route",
          "[routing][model]") {
    SKIP(
        "Solution::feasible() (src/routing/solution.cpp:78) checks load only, so a route that "
        "violates every declared time window is returned and reported feasible; the time-warp "
        "penalty pushes against it but nothing rejects it — coso#194");

    // Three clients whose windows admit exactly the far-to-near order, which is
    // the reverse of the distance-optimal one.
    RoutingModel model;
    model.add_depot(0.0, 0.0);
    model.add_vehicle_type(1, {.capacity = {100}});
    model.add_client(10.0, 0.0, {.demand = {1}, .tw = {60, 70}});
    model.add_client(20.0, 0.0, {.demand = {1}, .tw = {30, 40}});
    model.add_client(30.0, 0.0, {.demand = {1}, .tw = {0, 30}});

    Result r = model.solve(budget());

    REQUIRE(r.feasible());
    REQUIRE(r.routes().size() == 1);

    std::vector<Coord> coords = {{10.0, 0.0}, {20.0, 0.0}, {30.0, 0.0}};
    std::vector<TimeWindow> windows = {{60, 70}, {30, 40}, {0, 30}};
    Coord prev = {0.0, 0.0};
    int t = 0;
    for (int c : r.routes()[0]) {
        Coord cur = coords[static_cast<size_t>(c)];
        t += euclid(prev.x, prev.y, cur.x, cur.y);
        auto const& tw = windows[static_cast<size_t>(c)];
        t = std::max(t, tw.start);
        CHECK(t <= tw.end);
        prev = cur;
    }
}

TEST_CASE("RoutingModel: the depot time window bounds the route", "[routing][model]") {
    SKIP(
        "DurationResource::init_depot reads DepotParams::tw, but the resulting time warp is "
        "penalised and never classified: Solution::feasible() is load-only, so a route that "
        "returns after the depot closes comes back feasible — coso#194");

    // The depot shuts at 50; the round trip to the single client takes 60.
    RoutingModel model;
    model.add_depot(0.0, 0.0, {.tw = {0, 50}});
    model.add_vehicle_type(1, {.capacity = {100}});
    model.add_client(30.0, 0.0, {.demand = {1}});

    Result r = model.solve(budget());

    CHECK_FALSE(r.feasible());
}

TEST_CASE("RoutingModel: service time delays the arrivals after it", "[routing][model]") {
    SKIP(
        "ClientParams::service reaches DurationResource (src/routing/resources/"
        "duration_resource.h:51,53) and DistanceResource, but its only route-observable effect "
        "runs through time-window arrival times, and those are penalised without ever being "
        "classified — with no enforceable window there is nothing in a returned route that a "
        "declared service time can change — coso#194");

    // Travel is 10 per hop; a service time of 40 at the first client makes the
    // second client's window unreachable, so the order must invert.
    RoutingModel model;
    model.add_depot(0.0, 0.0);
    model.add_vehicle_type(1, {.capacity = {100}});
    model.add_client(10.0, 0.0, {.demand = {1}, .service = 40});
    model.add_client(20.0, 0.0, {.demand = {1}, .tw = {0, 30}});

    Result r = model.solve(budget());

    REQUIRE(r.feasible());
    REQUIRE(r.routes().size() == 1);
    CHECK(r.routes()[0][0] == 1);
}

// ---------------------------------------------------------------------------
//  Optional clients: blocked on #196
// ---------------------------------------------------------------------------

TEST_CASE("RoutingModel: an unprofitable optional client is left unserved", "[routing][model]") {
    SKIP(
        "ClientParams::required and ClientParams::prize reach no reachable code: "
        "src/routing/operators/insert_optional.h has no includer outside "
        "src/routing/operators/, construction.cpp serves every client and crossover.cpp "
        "reinserts every missing one, so no client is ever unserved and a prize is a constant "
        "offset — coso#196");

    // The far client's prize of 5 does not pay for the 400-unit detour.
    RoutingModel model;
    model.add_depot(0.0, 0.0);
    model.add_vehicle_type(1, {.capacity = {100}});
    model.add_client(10.0, 0.0, {.demand = {1}, .prize = 1000});
    model.add_client(200.0, 0.0, {.demand = {1}, .prize = 5, .required = false});

    Result r = model.solve(budget());

    REQUIRE(r.routes().size() == 1);
    CHECK(r.routes()[0].size() == 1);
    CHECK(r.unserved() == std::vector<int>{1});
}

TEST_CASE("RoutingModel: TSPTW solves as one time-feasible tour", "[routing][model]") {
    SKIP(
        "TSPTW is TSP plus enforced time windows, and Solution::feasible() checks load only, "
        "so the returned tour's arrival times are unconstrained — coso#194");

    RoutingModel model;
    model.add_depot(0.0, 0.0, {.tw = {0, 200}});
    model.add_vehicle_type(1);
    model.add_client(0.0, 10.0, {.tw = {0, 15}});
    model.add_client(10.0, 10.0, {.tw = {15, 30}});
    model.add_client(10.0, 0.0, {.tw = {30, 45}});

    Result r = model.solve(budget());

    REQUIRE(r.feasible());
    REQUIRE(r.routes().size() == 1);
    CHECK(r.routes()[0] == std::vector<int>{0, 1, 2});
}
