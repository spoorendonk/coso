#include "routing/resources/distance_resource.h"

#include "routing/route.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build test instances.
// ---------------------------------------------------------------------------

/// 1 depot at (0,0), 4 clients on a line: (10,0), (20,0), (30,0), (0,10).
/// Single vehicle type, no max_distance/max_duration constraints.
static ProblemData make_basic_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {100}});

    b.add_client({10.0, 0.0}, {.demand = {1}});  // client 0
    b.add_client({20.0, 0.0}, {.demand = {1}});  // client 1
    b.add_client({30.0, 0.0}, {.demand = {1}});  // client 2
    b.add_client({0.0, 10.0}, {.demand = {1}});  // client 3

    return b.build(0);
}

/// Instance with max_distance = 50 and max_duration = 60.
static ProblemData make_constrained_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {
                              .capacity = {100},
                              .max_duration = 60,
                              .max_distance = 50,
                          });

    b.add_client({10.0, 0.0}, {.demand = {1}, .service = 5});  // client 0
    b.add_client({20.0, 0.0}, {.demand = {1}, .service = 5});  // client 1
    b.add_client({30.0, 0.0}, {.demand = {1}, .service = 5});  // client 2
    b.add_client({0.0, 10.0}, {.demand = {1}, .service = 5});  // client 3

    return b.build(0);
}

/// Instance with only max_duration constraint (max_distance = 0 means unlimited).
static ProblemData make_duration_only_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(1, {
                              .capacity = {100},
                              .max_duration = 40,
                          });

    b.add_client({10.0, 0.0}, {.demand = {1}, .service = 10});  // client 0
    b.add_client({20.0, 0.0}, {.demand = {1}, .service = 10});  // client 1

    return b.build(0);
}

// ===========================================================================
//  DistanceResource unit tests
// ===========================================================================

TEST_CASE("DistanceResource::init creates correct single-client state", "[distance_resource]") {
    auto data = make_constrained_instance();

    auto s0 = DistanceResource::init(data, 0);
    CHECK(s0.distance == 0);
    CHECK(s0.duration == 5);  // service time
    CHECK(s0.first == 1);     // node index = num_depots + client = 1
    CHECK(s0.last == 1);
}

TEST_CASE("DistanceResource::init_depot creates empty state", "[distance_resource]") {
    auto s = DistanceResource::init_depot(0);
    CHECK(s.distance == 0);
    CHECK(s.duration == 0);
    CHECK(s.first == 0);
    CHECK(s.last == 0);
}

TEST_CASE("DistanceResource::merge combines two subsequences", "[distance_resource]") {
    auto data = make_basic_instance();

    // Clients at (10,0) and (20,0). Euclidean dist = 10.
    auto s0 = DistanceResource::init(data, 0);
    auto s1 = DistanceResource::init(data, 1);

    auto merged = DistanceResource::merge(s0, s1, data, 0);
    CHECK(merged.distance == 10);  // edge from c0 to c1
    CHECK(merged.duration == 10);  // edge duration (= dist by default) + 0 service
    CHECK(merged.first == 1);      // node index of c0
    CHECK(merged.last == 2);       // node index of c1
}

TEST_CASE("DistanceResource::merge with service times", "[distance_resource]") {
    auto data = make_constrained_instance();

    // Clients at (10,0) service=5 and (20,0) service=5. Edge dist = 10.
    auto s0 = DistanceResource::init(data, 0);
    auto s1 = DistanceResource::init(data, 1);

    auto merged = DistanceResource::merge(s0, s1, data, 0);
    CHECK(merged.distance == 10);  // just travel distance
    CHECK(merged.duration == 20);  // service(c0)=5 + travel=10 + service(c1)=5
}

TEST_CASE("DistanceResource::merge depot -> client -> depot", "[distance_resource]") {
    auto data = make_basic_instance();

    // Route: depot(0,0) -> c0(10,0) -> depot(0,0)
    auto depot = DistanceResource::init_depot(0);
    auto c0 = DistanceResource::init(data, 0);

    auto dep_c0 = DistanceResource::merge(depot, c0, data, 0);
    CHECK(dep_c0.distance == 10);  // depot -> c0

    auto full = DistanceResource::merge(dep_c0, DistanceResource::init_depot(0), data, 0);
    CHECK(full.distance == 20);  // depot -> c0 -> depot
}

TEST_CASE("DistanceResource::excess with no constraints returns 0", "[distance_resource]") {
    auto data = make_basic_instance();
    auto const& vt = data.vehicle_type(0);

    // max_distance = 0, max_duration = 0 means unlimited.
    DistanceResource::State s{.distance = 1000, .duration = 2000};
    CHECK(DistanceResource::excess(s, vt) == 0);
}

TEST_CASE("DistanceResource::excess with max_distance violation", "[distance_resource]") {
    auto data = make_constrained_instance();
    auto const& vt = data.vehicle_type(0);  // max_distance=50, max_duration=60

    DistanceResource::State s{.distance = 55, .duration = 40};
    CHECK(DistanceResource::excess(s, vt) == 5);  // 55 - 50 = 5
}

TEST_CASE("DistanceResource::excess with max_duration violation", "[distance_resource]") {
    auto data = make_constrained_instance();
    auto const& vt = data.vehicle_type(0);  // max_distance=50, max_duration=60

    DistanceResource::State s{.distance = 40, .duration = 75};
    CHECK(DistanceResource::excess(s, vt) == 15);  // 75 - 60 = 15
}

TEST_CASE("DistanceResource::excess with both violations", "[distance_resource]") {
    auto data = make_constrained_instance();
    auto const& vt = data.vehicle_type(0);  // max_distance=50, max_duration=60

    DistanceResource::State s{.distance = 60, .duration = 80};
    // (60-50) + (80-60) = 10 + 20 = 30
    CHECK(DistanceResource::excess(s, vt) == 30);
}

// ===========================================================================
//  Route integration tests for distance resource
// ===========================================================================

TEST_CASE("Route: empty route has zero duration and no dist excess", "[route][distance_resource]") {
    auto data = make_constrained_instance();
    Route route(data, 0);

    CHECK(route.distance() == 0);
    CHECK(route.duration() == 0);
    CHECK(route.dist_excess() == 0);
    CHECK(route.dist_feasible());
}

TEST_CASE("Route: distance and duration tracking", "[route][distance_resource]") {
    auto data = make_constrained_instance();
    Route route(data, 0);

    // Route: depot(0,0) -> c0(10,0) -> c1(20,0) -> depot(0,0)
    // Distance: 10 + 10 + 20 = 40
    // Duration: travel(10) + service(5) + travel(10) + service(5) + travel(20) = 50
    route.set_clients({0, 1});

    CHECK(route.distance() == 40);
    CHECK(route.duration() == 50);

    // max_distance=50, max_duration=60 -> feasible.
    CHECK(route.dist_feasible());
    CHECK(route.dist_excess() == 0);
}

TEST_CASE("Route: distance excess when max_distance exceeded", "[route][distance_resource]") {
    auto data = make_constrained_instance();
    Route route(data, 0);

    // Route: depot(0,0) -> c0(10,0) -> c1(20,0) -> c2(30,0) -> depot(0,0)
    // Distance: 10 + 10 + 10 + 30 = 60 > max_distance=50
    // Duration: 10 + 5 + 10 + 5 + 10 + 5 + 30 = 75 > max_duration=60
    route.set_clients({0, 1, 2});

    CHECK(route.distance() == 60);
    CHECK(route.duration() == 75);

    // Excess: (60-50) + (75-60) = 10 + 15 = 25
    CHECK(route.dist_excess() == 25);
    CHECK_FALSE(route.dist_feasible());
}

TEST_CASE("Route: duration-only constraint", "[route][distance_resource]") {
    auto data = make_duration_only_instance();
    Route route(data, 0);

    // Route: depot(0,0) -> c0(10,0) -> c1(20,0) -> depot(0,0)
    // Distance: 10 + 10 + 20 = 40
    // Duration: 10 + 10 + 10 + 10 + 20 = 60
    // max_duration=40 -> excess = 60 - 40 = 20
    route.set_clients({0, 1});

    CHECK(route.distance() == 40);
    CHECK(route.duration() == 60);
    CHECK(route.dist_excess() == 20);
    CHECK_FALSE(route.dist_feasible());
}

TEST_CASE("Route: dist prefix/suffix arrays are correct", "[route][distance_resource]") {
    auto data = make_constrained_instance();
    Route route(data, 0);

    route.set_clients({0, 1, 2});  // at (10,0), (20,0), (30,0), service=5 each

    // prefix(-1) = depot state.
    CHECK(route.dist_prefix(-1).distance == 0);
    CHECK(route.dist_prefix(-1).duration == 0);

    // prefix(0) = depot -> c0. dist=10, dur=10+5=15.
    CHECK(route.dist_prefix(0).distance == 10);
    CHECK(route.dist_prefix(0).duration == 15);

    // prefix(1) = depot -> c0 -> c1. dist=10+10=20, dur=15+10+5=30.
    CHECK(route.dist_prefix(1).distance == 20);
    CHECK(route.dist_prefix(1).duration == 30);

    // prefix(2) = depot -> c0 -> c1 -> c2. dist=30, dur=45.
    CHECK(route.dist_prefix(2).distance == 30);
    CHECK(route.dist_prefix(2).duration == 45);

    // suffix(3) = depot state (after all clients).
    CHECK(route.dist_suffix(3).distance == 0);
    CHECK(route.dist_suffix(3).duration == 0);

    // suffix(2) = c2 -> depot. dist=30, dur=5+30=35.
    CHECK(route.dist_suffix(2).distance == 30);
    CHECK(route.dist_suffix(2).duration == 35);
}

// ===========================================================================
//  O(1) move evaluation tests for distance resource
// ===========================================================================

TEST_CASE("Route: eval_insert_dist_excess matches actual insert", "[route][distance_resource]") {
    auto data = make_constrained_instance();
    Route route(data, 0);

    route.set_clients({0, 1});

    // Evaluate inserting client 2 at every position.
    for (int pos = 0; pos <= route.size(); ++pos) {
        int predicted = route.eval_insert_dist_excess(pos, 2);

        Route copy(data, 0);
        copy.set_clients({0, 1});
        copy.insert(pos, 2);

        CHECK(predicted == copy.dist_excess());
    }
}

TEST_CASE("Route: eval_remove_dist_excess matches actual remove", "[route][distance_resource]") {
    auto data = make_constrained_instance();
    Route route(data, 0);

    route.set_clients({0, 1, 2});

    for (int pos = 0; pos < route.size(); ++pos) {
        int predicted = route.eval_remove_dist_excess(pos);

        Route copy(data, 0);
        copy.set_clients({0, 1, 2});
        copy.remove(pos);

        CHECK(predicted == copy.dist_excess());
    }
}

TEST_CASE("Route: eval_insert_dist_excess on empty route", "[route][distance_resource]") {
    auto data = make_constrained_instance();
    Route route(data, 0);

    int predicted = route.eval_insert_dist_excess(0, 0);

    Route verify(data, 0);
    verify.insert(0, 0);

    CHECK(predicted == verify.dist_excess());
}

TEST_CASE("Route: eval_insert_dist_excess with all clients and positions",
          "[route][distance_resource]") {
    auto data = make_constrained_instance();

    // Test all combinations of existing routes and insertions.
    for (int base_size = 0; base_size <= 3; ++base_size) {
        std::vector<int> base_clients;
        for (int i = 0; i < base_size; ++i) {
            base_clients.push_back(i);
        }

        Route route(data, 0);
        route.set_clients(base_clients);

        // Try inserting client 3 at every position.
        for (int pos = 0; pos <= route.size(); ++pos) {
            int predicted = route.eval_insert_dist_excess(pos, 3);

            Route copy(data, 0);
            copy.set_clients(base_clients);
            copy.insert(pos, 3);

            CHECK(predicted == copy.dist_excess());
        }
    }
}

TEST_CASE("Route: distance unchanged from original computation", "[route][distance_resource]") {
    // Verify the new DistanceResource-based distance matches the original
    // simple sum-of-edges computation.
    auto data = make_basic_instance();
    Route route(data, 0);

    // Route: depot(0,0) -> c0(10,0) -> c1(20,0) -> c2(30,0) -> depot(0,0)
    // Distance: 10 + 10 + 10 + 30 = 60
    route.set_clients({0, 1, 2});
    CHECK(route.distance() == 60);

    // Route: depot(0,0) -> c3(0,10) -> depot(0,0)
    // Distance: 10 + 10 = 20
    route.set_clients({3});
    CHECK(route.distance() == 20);

    // Route: depot(0,0) -> c0(10,0) -> c3(0,10) -> depot(0,0)
    // Distance: 10 + round(sqrt(100+100)) + 10 = 10 + 14 + 10 = 34
    route.set_clients({0, 3});
    CHECK(route.distance() == 34);
}
