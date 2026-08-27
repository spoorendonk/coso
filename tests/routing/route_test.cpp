#include "routing/route.h"

#include "routing/resources/load_resource.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a small CVRP instance for testing.
// ---------------------------------------------------------------------------

/// 1 depot at (0,0), clients at (10,0), (20,0), (30,0), (0,10).
/// Single vehicle type with capacity 10, 1 load dimension.
static ProblemData make_small_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {10}});

    b.add_client({10.0, 0.0}, {.demand = {3}});  // client 0
    b.add_client({20.0, 0.0}, {.demand = {4}});  // client 1
    b.add_client({30.0, 0.0}, {.demand = {5}});  // client 2
    b.add_client({0.0, 10.0}, {.demand = {2}});  // client 3

    return b.build(0);  // no granular neighbours
}

/// Multi-dimensional load: 1 depot, 3 clients, capacity {10, 8}.
static ProblemData make_multidim_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(1, {.capacity = {10, 8}});

    b.add_client({10.0, 0.0}, {.demand = {3, 2}});  // client 0
    b.add_client({20.0, 0.0}, {.demand = {4, 5}});  // client 1
    b.add_client({30.0, 0.0}, {.demand = {5, 3}});  // client 2

    return b.build(0);
}

/// Instance with pickup quantities.
static ProblemData make_pickup_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(1, {.capacity = {10}});

    b.add_client({10.0, 0.0}, {.demand = {3}, .pickup = {1}});  // client 0
    b.add_client({20.0, 0.0}, {.demand = {4}, .pickup = {2}});  // client 1
    b.add_client({30.0, 0.0}, {.demand = {2}, .pickup = {3}});  // client 2

    return b.build(0);
}

// ===========================================================================
//  LoadResource tests
// ===========================================================================

TEST_CASE("LoadResource::init creates correct single-client state", "[load_resource]") {
    auto data = make_small_instance();

    auto s0 = LoadResource::init(data, 0);
    REQUIRE(s0.num_dims() == 1);
    CHECK(s0.dims[0].delivery == 3);
    CHECK(s0.dims[0].pickup == 0);
    CHECK(s0.dims[0].load == 3);

    auto s2 = LoadResource::init(data, 2);
    CHECK(s2.dims[0].delivery == 5);
    CHECK(s2.dims[0].load == 5);
}

TEST_CASE("LoadResource::init_depot creates empty state", "[load_resource]") {
    auto data = make_small_instance();
    auto s = LoadResource::init_depot(data);
    REQUIRE(s.num_dims() == 1);
    CHECK(s.dims[0].delivery == 0);
    CHECK(s.dims[0].pickup == 0);
    CHECK(s.dims[0].load == 0);
}

TEST_CASE("LoadResource::merge combines two delivery-only subsequences", "[load_resource]") {
    auto data = make_small_instance();

    // Clients 0 (demand=3) and 1 (demand=4), total demand = 7.
    auto s0 = LoadResource::init(data, 0);
    auto s1 = LoadResource::init(data, 1);

    auto merged = LoadResource::merge(s0, s1);
    CHECK(merged.dims[0].delivery == 7);
    CHECK(merged.dims[0].pickup == 0);
    // Vehicle starts with all delivery loaded: load = 7.
    // After delivering client 0 (3): remaining = 4. Then deliver client 1.
    // Max load = 7 (at start, before any delivery).
    CHECK(merged.dims[0].load == 7);
}

TEST_CASE("LoadResource::merge three clients", "[load_resource]") {
    auto data = make_small_instance();

    // Clients 0 (3), 1 (4), 2 (5) -> total = 12.
    auto s0 = LoadResource::init(data, 0);
    auto s1 = LoadResource::init(data, 1);
    auto s2 = LoadResource::init(data, 2);

    auto m01 = LoadResource::merge(s0, s1);
    auto m012 = LoadResource::merge(m01, s2);

    CHECK(m012.dims[0].delivery == 12);
    CHECK(m012.dims[0].load == 12);
}

TEST_CASE("LoadResource::merge with pickups", "[load_resource]") {
    auto data = make_pickup_instance();

    // Client 0: demand=3, pickup=1 -> load=4 (delivery + pickup at that node)
    // Client 1: demand=4, pickup=2 -> load=6
    auto s0 = LoadResource::init(data, 0);
    auto s1 = LoadResource::init(data, 1);

    CHECK(s0.dims[0].delivery == 3);
    CHECK(s0.dims[0].pickup == 1);
    CHECK(s0.dims[0].load == 4);  // demand + pickup

    auto merged = LoadResource::merge(s0, s1);
    CHECK(merged.dims[0].delivery == 7);  // 3 + 4
    CHECK(merged.dims[0].pickup == 3);    // 1 + 2

    // Max load in left part: left.load + right.delivery = 4 + 4 = 8
    //   (vehicle carries all delivery + pickup from left, plus right's delivery)
    // Max load in right part: right.load + left.pickup = 6 + 1 = 7
    CHECK(merged.dims[0].load == 8);
}

TEST_CASE("LoadResource::excess with single dimension", "[load_resource]") {
    auto data = make_small_instance();

    // Route with clients 0 (3), 1 (4) -> total load = 7.
    auto s0 = LoadResource::init(data, 0);
    auto s1 = LoadResource::init(data, 1);
    auto merged = LoadResource::merge(s0, s1);

    // Capacity = 10, load = 7 -> no excess.
    CHECK(LoadResource::excess(merged, data.vehicle_type(0)) == 0);

    // Route with clients 0 (3), 1 (4), 2 (5) -> total load = 12.
    auto s2 = LoadResource::init(data, 2);
    auto m012 = LoadResource::merge(merged, s2);

    // Capacity = 10, load = 12 -> excess = 2.
    CHECK(LoadResource::excess(m012, data.vehicle_type(0)) == 2);
}

TEST_CASE("LoadResource::excess with multi-dimensional loads", "[load_resource]") {
    auto data = make_multidim_instance();

    // All 3 clients: demand = {3+4+5, 2+5+3} = {12, 10}.
    // Capacity = {10, 8}.
    auto s0 = LoadResource::init(data, 0);
    auto s1 = LoadResource::init(data, 1);
    auto s2 = LoadResource::init(data, 2);

    auto m01 = LoadResource::merge(s0, s1);
    auto m012 = LoadResource::merge(m01, s2);

    CHECK(m012.dims[0].delivery == 12);
    CHECK(m012.dims[1].delivery == 10);

    // Excess: dim0 = 12 - 10 = 2, dim1 = 10 - 8 = 2, total = 4.
    CHECK(LoadResource::excess(m012, data.vehicle_type(0)) == 4);
}

TEST_CASE("LoadResource::merge_reverse equals merge (direction-independent)", "[load_resource]") {
    auto data = make_pickup_instance();

    auto s0 = LoadResource::init(data, 0);
    auto s1 = LoadResource::init(data, 1);

    auto fwd = LoadResource::merge(s0, s1);
    auto rev = LoadResource::merge_reverse(s0, s1);

    REQUIRE(fwd.num_dims() == rev.num_dims());
    for (int d = 0; d < fwd.num_dims(); ++d) {
        CHECK(fwd.dims[d].delivery == rev.dims[d].delivery);
        CHECK(fwd.dims[d].pickup == rev.dims[d].pickup);
        CHECK(fwd.dims[d].load == rev.dims[d].load);
    }
}

// ===========================================================================
//  Route construction and resource tracking tests
// ===========================================================================

TEST_CASE("Route: empty route has zero load and distance", "[route]") {
    auto data = make_small_instance();
    Route route(data, 0);

    CHECK(route.size() == 0);
    CHECK(route.empty());
    CHECK(route.load_excess() == 0);
    CHECK(route.load_feasible());
    CHECK(route.distance() == 0);
}

TEST_CASE("Route: set_clients updates resources correctly", "[route]") {
    auto data = make_small_instance();
    Route route(data, 0);

    // Clients 0 (demand=3) and 1 (demand=4), total = 7 <= capacity 10.
    route.set_clients({0, 1});

    CHECK(route.size() == 2);
    CHECK(route.client(0) == 0);
    CHECK(route.client(1) == 1);
    CHECK(route.load_feasible());
    CHECK(route.load_excess() == 0);

    // Distance: depot(0,0) -> c0(10,0) -> c1(20,0) -> depot(0,0) = 10+10+20 = 40.
    CHECK(route.distance() == 40);
}

TEST_CASE("Route: set_clients with excess load", "[route]") {
    auto data = make_small_instance();
    Route route(data, 0);

    // Clients 0 (3), 1 (4), 2 (5) -> total = 12 > capacity 10.
    route.set_clients({0, 1, 2});

    CHECK(route.size() == 3);
    CHECK_FALSE(route.load_feasible());
    CHECK(route.load_excess() == 2);
}

TEST_CASE("Route: insert adds client and updates resources", "[route]") {
    auto data = make_small_instance();
    Route route(data, 0);

    route.set_clients({0, 1});
    CHECK(route.size() == 2);

    // Insert client 3 (demand=2) at position 1.
    route.insert(1, 3);

    CHECK(route.size() == 3);
    CHECK(route.client(0) == 0);
    CHECK(route.client(1) == 3);
    CHECK(route.client(2) == 1);

    // Total demand: 3 + 2 + 4 = 9 <= 10.
    CHECK(route.load_feasible());
}

TEST_CASE("Route: insert at beginning and end", "[route]") {
    auto data = make_small_instance();
    Route route(data, 0);

    route.set_clients({1});  // client 1 (demand=4)

    // Insert at beginning.
    route.insert(0, 0);  // client 0 (demand=3)
    CHECK(route.client(0) == 0);
    CHECK(route.client(1) == 1);

    // Insert at end.
    route.insert(2, 3);  // client 3 (demand=2)
    CHECK(route.client(2) == 3);
    CHECK(route.size() == 3);

    // Total demand: 3 + 4 + 2 = 9 <= 10.
    CHECK(route.load_feasible());
}

TEST_CASE("Route: remove client updates resources", "[route]") {
    auto data = make_small_instance();
    Route route(data, 0);

    // Overloaded: 3 + 4 + 5 = 12 > 10.
    route.set_clients({0, 1, 2});
    CHECK(route.load_excess() == 2);

    // Remove client 2 (demand=5): 3 + 4 = 7 <= 10.
    route.remove(2);
    CHECK(route.size() == 2);
    CHECK(route.load_feasible());
    CHECK(route.load_excess() == 0);
}

TEST_CASE("Route: replace client updates resources", "[route]") {
    auto data = make_small_instance();
    Route route(data, 0);

    // Clients 0 (3), 2 (5) -> total = 8 <= 10.
    route.set_clients({0, 2});
    CHECK(route.load_feasible());

    // Replace client 2 (demand=5) with client 1 (demand=4).
    route.replace(1, 1);
    CHECK(route.client(1) == 1);
    // Total: 3 + 4 = 7.
    CHECK(route.load_feasible());
}

TEST_CASE("Route: prefix/suffix arrays are correct", "[route]") {
    auto data = make_small_instance();
    Route route(data, 0);

    route.set_clients({0, 1, 2});  // demands: 3, 4, 5

    // prefix(-1) = depot (empty).
    CHECK(route.load_prefix(-1).dims[0].delivery == 0);

    // prefix(0) = just client 0.
    CHECK(route.load_prefix(0).dims[0].delivery == 3);

    // prefix(1) = clients 0, 1.
    CHECK(route.load_prefix(1).dims[0].delivery == 7);

    // prefix(2) = clients 0, 1, 2 (full route).
    CHECK(route.load_prefix(2).dims[0].delivery == 12);

    // suffix(3) = empty (after last client).
    CHECK(route.load_suffix(3).dims[0].delivery == 0);

    // suffix(2) = just client 2.
    CHECK(route.load_suffix(2).dims[0].delivery == 5);

    // suffix(1) = clients 1, 2.
    CHECK(route.load_suffix(1).dims[0].delivery == 9);

    // suffix(0) = clients 0, 1, 2 (full route).
    CHECK(route.load_suffix(0).dims[0].delivery == 12);
}

// ===========================================================================
//  O(1) move evaluation tests
// ===========================================================================

TEST_CASE("Route: eval_insert_load matches actual insert", "[route]") {
    auto data = make_small_instance();
    Route route(data, 0);

    route.set_clients({0, 1});  // demands: 3, 4

    // Evaluate inserting client 2 (demand=5) at every position.
    for (int pos = 0; pos <= route.size(); ++pos) {
        int predicted_excess = route.eval_insert_load(pos, 2);

        // Actually insert and check.
        Route copy(data, 0);
        copy.set_clients({0, 1});
        copy.insert(pos, 2);

        CHECK(predicted_excess == copy.load_excess());
    }
}

TEST_CASE("Route: eval_remove_load matches actual remove", "[route]") {
    auto data = make_small_instance();
    Route route(data, 0);

    route.set_clients({0, 1, 2});  // demands: 3, 4, 5

    for (int pos = 0; pos < route.size(); ++pos) {
        int predicted_excess = route.eval_remove_load(pos);

        Route copy(data, 0);
        copy.set_clients({0, 1, 2});
        copy.remove(pos);

        CHECK(predicted_excess == copy.load_excess());
    }
}

TEST_CASE("Route: eval_insert_distance matches actual insert", "[route]") {
    auto data = make_small_instance();
    Route route(data, 0);

    route.set_clients({0, 1});

    for (int pos = 0; pos <= route.size(); ++pos) {
        int delta = route.eval_insert_distance(pos, 2);

        Route copy(data, 0);
        copy.set_clients({0, 1});
        int old_dist = copy.distance();
        copy.insert(pos, 2);
        int new_dist = copy.distance();

        CHECK(delta == new_dist - old_dist);
    }
}

TEST_CASE("Route: eval_remove_distance matches actual remove", "[route]") {
    auto data = make_small_instance();
    Route route(data, 0);

    route.set_clients({0, 1, 2});

    for (int pos = 0; pos < route.size(); ++pos) {
        int delta = route.eval_remove_distance(pos);

        Route copy(data, 0);
        copy.set_clients({0, 1, 2});
        int old_dist = copy.distance();
        copy.remove(pos);
        int new_dist = copy.distance();

        CHECK(delta == new_dist - old_dist);
    }
}

TEST_CASE("Route: eval_insert_load with multi-dim loads", "[route]") {
    auto data = make_multidim_instance();
    Route route(data, 0);

    route.set_clients({0});  // demands: {3, 2}

    // Insert client 1 {4, 5} at position 1.
    int excess = route.eval_insert_load(1, 1);

    Route verify(data, 0);
    verify.set_clients({0, 1});

    CHECK(excess == verify.load_excess());
}

TEST_CASE("Route: eval_insert_load with pickups", "[route]") {
    auto data = make_pickup_instance();
    Route route(data, 0);

    route.set_clients({0});  // demand=3, pickup=1

    for (int pos = 0; pos <= route.size(); ++pos) {
        int excess = route.eval_insert_load(pos, 1);  // demand=4, pickup=2

        Route copy(data, 0);
        copy.set_clients({0});
        copy.insert(pos, 1);

        CHECK(excess == copy.load_excess());
    }
}

TEST_CASE("Route: insert into empty route", "[route]") {
    auto data = make_small_instance();
    Route route(data, 0);

    route.insert(0, 0);  // client 0 (demand=3)
    CHECK(route.size() == 1);
    CHECK(route.client(0) == 0);
    CHECK(route.load_feasible());

    // Distance: depot(0,0) -> c0(10,0) -> depot(0,0) = 10 + 10 = 20.
    CHECK(route.distance() == 20);
}

TEST_CASE("Route: eval on empty route", "[route]") {
    auto data = make_small_instance();
    Route route(data, 0);

    // Insert client 0 into empty route.
    int excess = route.eval_insert_load(0, 0);
    CHECK(excess == 0);  // demand=3, capacity=10

    int dist_delta = route.eval_insert_distance(0, 0);
    // depot -> c0 -> depot = 20, was 0.
    CHECK(dist_delta == 20);
}

TEST_CASE("Route: distance computation with collinear clients", "[route]") {
    auto data = make_small_instance();
    Route route(data, 0);

    // Route: depot(0,0) -> c0(10,0) -> c1(20,0) -> c2(30,0) -> depot(0,0).
    route.set_clients({0, 1, 2});

    // Distances (Euclidean, rounded):
    // depot->c0=10, c0->c1=10, c1->c2=10, c2->depot=30.
    CHECK(route.distance() == 60);
}

TEST_CASE("Route: clients() span access", "[route]") {
    auto data = make_small_instance();
    Route route(data, 0);

    route.set_clients({2, 0, 1});

    auto span = route.clients();
    REQUIRE(span.size() == 3);
    CHECK(span[0] == 2);
    CHECK(span[1] == 0);
    CHECK(span[2] == 1);
}
