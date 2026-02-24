#include "routing/resources/depot_resource.h"

#include <catch2/catch_test_macros.hpp>
#include <climits>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a simple multi-depot problem
// ---------------------------------------------------------------------------

/// Creates a problem with 2 depots and 3 clients.
/// Depot 0: tw = [100, 500]
/// Depot 1: tw = [200, 800]
/// Clients: 3 clients with unit demand.
static ProblemData make_two_depot_problem() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0}, {{100, 500}});  // depot 0
    b.add_depot({10.0, 0.0}, {{200, 800}}); // depot 1
    b.add_client({1.0, 0.0}, {.demand = {1}});
    b.add_client({2.0, 0.0}, {.demand = {1}});
    b.add_client({3.0, 0.0}, {.demand = {1}});
    b.add_vehicle_type(2, {.capacity = {10}});
    return b.build(0);
}

/// Creates a single-depot problem.
/// Depot 0: tw = [0, 1000]
static ProblemData make_single_depot_problem() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0}, {{0, 1000}});
    b.add_client({1.0, 0.0}, {.demand = {1}});
    b.add_client({2.0, 0.0}, {.demand = {1}});
    b.add_vehicle_type(1, {.capacity = {10}});
    return b.build(0);
}

// ---------------------------------------------------------------------------
//  init_depot
// ---------------------------------------------------------------------------

TEST_CASE("DepotResource: init_depot sets correct state", "[depot_resource]") {
    auto data = make_two_depot_problem();

    SECTION("depot 0") {
        auto s = DepotResource::init_depot(data, 0);
        REQUIRE(s.depot == 0);
        REQUIRE(s.depart == 100);
        REQUIRE(s.arrive_back == 100);
        REQUIRE(s.tw_open == 100);
        REQUIRE(s.tw_close == 500);
    }

    SECTION("depot 1") {
        auto s = DepotResource::init_depot(data, 1);
        REQUIRE(s.depot == 1);
        REQUIRE(s.depart == 200);
        REQUIRE(s.arrive_back == 200);
        REQUIRE(s.tw_open == 200);
        REQUIRE(s.tw_close == 800);
    }
}

TEST_CASE("DepotResource: init_depot with default TW", "[depot_resource]") {
    auto data = make_single_depot_problem();
    auto s = DepotResource::init_depot(data, 0);

    REQUIRE(s.depot == 0);
    REQUIRE(s.depart == 0);
    REQUIRE(s.tw_open == 0);
    REQUIRE(s.tw_close == 1000);
}

// ---------------------------------------------------------------------------
//  init (client)
// ---------------------------------------------------------------------------

TEST_CASE("DepotResource: init for client has no depot", "[depot_resource]") {
    auto data = make_two_depot_problem();
    auto s = DepotResource::init(data, 0);

    REQUIRE(s.depot == -1);
    REQUIRE_FALSE(DepotResource::has_depot(s));
}

// ---------------------------------------------------------------------------
//  merge
// ---------------------------------------------------------------------------

TEST_CASE("DepotResource: merge prefers left depot", "[depot_resource]") {
    auto data = make_two_depot_problem();

    auto left  = DepotResource::init_depot(data, 0);
    auto right = DepotResource::init_depot(data, 1);

    auto merged = DepotResource::merge(left, right);
    REQUIRE(merged.depot == 0);  // left wins
    REQUIRE(merged.tw_open == 100);
    REQUIRE(merged.tw_close == 500);
}

TEST_CASE("DepotResource: merge with unassigned left uses right", "[depot_resource]") {
    auto data = make_two_depot_problem();

    auto left  = DepotResource::init(data, 0);  // no depot
    auto right = DepotResource::init_depot(data, 1);

    auto merged = DepotResource::merge(left, right);
    REQUIRE(merged.depot == 1);
    REQUIRE(merged.tw_open == 200);
    REQUIRE(merged.tw_close == 800);
}

TEST_CASE("DepotResource: merge two unassigned states", "[depot_resource]") {
    auto data = make_two_depot_problem();

    auto left  = DepotResource::init(data, 0);
    auto right = DepotResource::init(data, 1);

    auto merged = DepotResource::merge(left, right);
    REQUIRE(merged.depot == -1);
    REQUIRE_FALSE(DepotResource::has_depot(merged));
}

TEST_CASE("DepotResource: merge keeps max arrive_back", "[depot_resource]") {
    auto data = make_two_depot_problem();

    auto left = DepotResource::init_depot(data, 0);
    auto right = DepotResource::init_depot(data, 1);

    // Simulate a route that returns late.
    auto late_right = DepotResource::with_return_time(right, 700);
    auto merged = DepotResource::merge(left, late_right);

    REQUIRE(merged.arrive_back == 700);
}

// ---------------------------------------------------------------------------
//  with_return_time
// ---------------------------------------------------------------------------

TEST_CASE("DepotResource: with_return_time updates arrival", "[depot_resource]") {
    auto data = make_two_depot_problem();
    auto s = DepotResource::init_depot(data, 0);

    REQUIRE(s.arrive_back == 100);

    auto updated = DepotResource::with_return_time(s, 450);
    REQUIRE(updated.arrive_back == 450);
    REQUIRE(updated.depot == 0);  // unchanged
    REQUIRE(updated.depart == 100);  // unchanged
}

// ---------------------------------------------------------------------------
//  excess — feasible cases
// ---------------------------------------------------------------------------

TEST_CASE("DepotResource: no excess when within TW", "[depot_resource]") {
    auto data = make_two_depot_problem();

    SECTION("depart at open, return before close") {
        auto s = DepotResource::init_depot(data, 0);  // tw [100, 500]
        auto updated = DepotResource::with_return_time(s, 400);
        REQUIRE(DepotResource::excess(updated) == 0);
        REQUIRE(DepotResource::is_feasible(updated));
    }

    SECTION("depart at open, return exactly at close") {
        auto s = DepotResource::init_depot(data, 0);
        auto updated = DepotResource::with_return_time(s, 500);
        REQUIRE(DepotResource::excess(updated) == 0);
        REQUIRE(DepotResource::is_feasible(updated));
    }

    SECTION("depot 1 within TW") {
        auto s = DepotResource::init_depot(data, 1);  // tw [200, 800]
        auto updated = DepotResource::with_return_time(s, 600);
        REQUIRE(DepotResource::excess(updated) == 0);
    }
}

// ---------------------------------------------------------------------------
//  excess — violation cases
// ---------------------------------------------------------------------------

TEST_CASE("DepotResource: late return excess", "[depot_resource]") {
    auto data = make_two_depot_problem();

    auto s = DepotResource::init_depot(data, 0);  // tw [100, 500]
    auto updated = DepotResource::with_return_time(s, 550);

    REQUIRE(DepotResource::excess(updated) == 50);
    REQUIRE_FALSE(DepotResource::is_feasible(updated));
}

TEST_CASE("DepotResource: large late return excess", "[depot_resource]") {
    auto data = make_two_depot_problem();

    auto s = DepotResource::init_depot(data, 1);  // tw [200, 800]
    auto updated = DepotResource::with_return_time(s, 1200);

    REQUIRE(DepotResource::excess(updated) == 400);
}

TEST_CASE("DepotResource: early departure excess", "[depot_resource]") {
    auto data = make_two_depot_problem();

    // Manually construct a state with departure before open.
    DepotResource::State s;
    s.depot       = 0;
    s.depart      = 50;   // before tw_open = 100
    s.arrive_back = 400;
    s.tw_open     = 100;
    s.tw_close    = 500;

    REQUIRE(DepotResource::excess(s) == 50);
}

TEST_CASE("DepotResource: both early departure and late return", "[depot_resource]") {
    DepotResource::State s;
    s.depot       = 0;
    s.depart      = 50;
    s.arrive_back = 600;
    s.tw_open     = 100;
    s.tw_close    = 500;

    // Early: 100 - 50 = 50, Late: 600 - 500 = 100
    REQUIRE(DepotResource::excess(s) == 150);
}

// ---------------------------------------------------------------------------
//  excess with VehicleTypeData overload
// ---------------------------------------------------------------------------

TEST_CASE("DepotResource: excess with vehicle type delegates correctly", "[depot_resource]") {
    auto data = make_two_depot_problem();

    auto s = DepotResource::init_depot(data, 0);
    auto updated = DepotResource::with_return_time(s, 550);

    auto const& vt = data.vehicle_type(0);
    REQUIRE(DepotResource::excess(updated, vt) == 50);
}

// ---------------------------------------------------------------------------
//  has_depot / is_feasible
// ---------------------------------------------------------------------------

TEST_CASE("DepotResource: has_depot checks assignment", "[depot_resource]") {
    auto data = make_two_depot_problem();

    auto assigned = DepotResource::init_depot(data, 0);
    REQUIRE(DepotResource::has_depot(assigned));

    auto unassigned = DepotResource::init(data, 0);
    REQUIRE_FALSE(DepotResource::has_depot(unassigned));
}

TEST_CASE("DepotResource: unassigned state is feasible (no depot TW to violate)", "[depot_resource]") {
    auto data = make_two_depot_problem();
    auto s = DepotResource::init(data, 0);
    REQUIRE(DepotResource::is_feasible(s));
}

// ---------------------------------------------------------------------------
//  Edge case: INT_MAX time window
// ---------------------------------------------------------------------------

TEST_CASE("DepotResource: unconstrained depot (INT_MAX close)", "[depot_resource]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});  // default TW: [0, INT_MAX]
    b.add_client({1.0, 0.0});
    b.add_vehicle_type(1);
    auto data = b.build(0);

    auto s = DepotResource::init_depot(data, 0);
    REQUIRE(s.tw_close == INT_MAX);

    auto updated = DepotResource::with_return_time(s, 999999);
    REQUIRE(DepotResource::excess(updated) == 0);
    REQUIRE(DepotResource::is_feasible(updated));
}
