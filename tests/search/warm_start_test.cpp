#include "search/warm_start.h"

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a small CVRP instance (6 clients, 1 depot, 3 vehicles cap 20)
// ---------------------------------------------------------------------------

static ProblemData make_small_cvrp() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});

    b.add_client({10.0, 0.0}, {.demand = {5}});     // 0
    b.add_client({10.0, 10.0}, {.demand = {5}});    // 1
    b.add_client({0.0, 10.0}, {.demand = {5}});     // 2
    b.add_client({-10.0, 0.0}, {.demand = {5}});    // 3
    b.add_client({-10.0, -10.0}, {.demand = {5}});  // 4
    b.add_client({0.0, -10.0}, {.demand = {5}});    // 5

    b.add_vehicle_type(3, {.capacity = {20}});

    return b.build();
}

// ---------------------------------------------------------------------------
//  Helper: instance with optional clients
// ---------------------------------------------------------------------------

static ProblemData make_optional_cvrp() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});

    b.add_client({10.0, 0.0}, {.demand = {5}});                                   // 0 required
    b.add_client({10.0, 10.0}, {.demand = {5}});                                  // 1 required
    b.add_client({0.0, 10.0}, {.demand = {5}, .prize = 50, .required = false});   // 2 optional
    b.add_client({-10.0, 0.0}, {.demand = {5}, .prize = 50, .required = false});  // 3 optional

    b.add_vehicle_type(2, {.capacity = {20}});

    return b.build();
}

// ===========================================================================
//  PinSet tests
// ===========================================================================

TEST_CASE("PinSet — default construction is empty", "[warm_start][pinset]") {
    PinSet pins;
    CHECK(pins.empty());
    CHECK(pins.size() == 0);
    CHECK_FALSE(pins.is_pinned(0));
    CHECK_FALSE(pins.is_pinned(999));
}

TEST_CASE("PinSet — sized construction", "[warm_start][pinset]") {
    PinSet pins(10);
    CHECK(pins.empty());
    CHECK_FALSE(pins.is_pinned(0));
    CHECK_FALSE(pins.is_pinned(9));
}

TEST_CASE("PinSet — pin and query", "[warm_start][pinset]") {
    PinSet pins(6);

    pins.pin(2);
    pins.pin(4);

    CHECK(pins.is_pinned(2));
    CHECK(pins.is_pinned(4));
    CHECK_FALSE(pins.is_pinned(0));
    CHECK_FALSE(pins.is_pinned(1));
    CHECK_FALSE(pins.is_pinned(3));
    CHECK_FALSE(pins.is_pinned(5));

    CHECK(pins.size() == 2);
    CHECK(pins.pinned() == std::vector<int>{2, 4});
}

TEST_CASE("PinSet — pin is idempotent", "[warm_start][pinset]") {
    PinSet pins(6);

    pins.pin(3);
    pins.pin(3);  // no-op
    pins.pin(3);  // no-op

    CHECK(pins.size() == 1);
    CHECK(pins.is_pinned(3));
}

TEST_CASE("PinSet — unpin", "[warm_start][pinset]") {
    PinSet pins(6);

    pins.pin(1);
    pins.pin(3);
    pins.pin(5);
    CHECK(pins.size() == 3);

    pins.unpin(3);
    CHECK_FALSE(pins.is_pinned(3));
    CHECK(pins.size() == 2);
    CHECK(pins.pinned() == std::vector<int>{1, 5});

    // Unpin non-pinned client is a no-op.
    pins.unpin(3);
    CHECK(pins.size() == 2);

    // Unpin out-of-range client is a no-op.
    pins.unpin(100);
    CHECK(pins.size() == 2);
}

TEST_CASE("PinSet — clear", "[warm_start][pinset]") {
    PinSet pins(6);
    pins.pin(0);
    pins.pin(2);
    pins.pin(4);

    pins.clear();
    CHECK(pins.empty());
    CHECK_FALSE(pins.is_pinned(0));
    CHECK_FALSE(pins.is_pinned(2));
    CHECK_FALSE(pins.is_pinned(4));
}

TEST_CASE("PinSet — auto-grow on pin beyond initial size", "[warm_start][pinset]") {
    PinSet pins;  // default, no initial size

    pins.pin(42);
    CHECK(pins.is_pinned(42));
    CHECK(pins.size() == 1);
    CHECK_FALSE(pins.is_pinned(0));
    CHECK_FALSE(pins.is_pinned(41));
}

TEST_CASE("PinSet — pinned list is sorted", "[warm_start][pinset]") {
    PinSet pins(10);

    pins.pin(7);
    pins.pin(2);
    pins.pin(9);
    pins.pin(0);
    pins.pin(5);

    CHECK(pins.pinned() == std::vector<int>{0, 2, 5, 7, 9});
}

TEST_CASE("PinSet — invalid client throws", "[warm_start][pinset]") {
    PinSet pins(6);
    CHECK_THROWS_AS(pins.pin(-1), std::invalid_argument);
}

// ===========================================================================
//  warm_start tests
// ===========================================================================

TEST_CASE("warm_start — valid routes produce correct solution", "[warm_start]") {
    auto data = make_small_cvrp();
    CostEvaluator eval;

    // Two routes covering all 6 clients.
    std::vector<std::vector<int>> routes = {{0, 1, 2}, {3, 4, 5}};

    Solution sol = warm_start(routes, data, eval);

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.route(0).size() == 3);
    CHECK(sol.route(1).size() == 3);
    CHECK(sol.route(2).size() == 0);  // third vehicle unused

    // Verify client assignments match.
    CHECK(sol.route(0).client(0) == 0);
    CHECK(sol.route(0).client(1) == 1);
    CHECK(sol.route(0).client(2) == 2);
    CHECK(sol.route(1).client(0) == 3);
    CHECK(sol.route(1).client(1) == 4);
    CHECK(sol.route(1).client(2) == 5);
}

TEST_CASE("warm_start — single route with all clients", "[warm_start]") {
    auto data = make_small_cvrp();
    CostEvaluator eval;

    std::vector<std::vector<int>> routes = {{0, 1, 2, 3, 4, 5}};

    Solution sol = warm_start(routes, data, eval);

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.route(0).size() == 6);
    CHECK(sol.num_used_vehicles() == 1);
}

TEST_CASE("warm_start — empty routes are allowed", "[warm_start]") {
    auto data = make_small_cvrp();
    CostEvaluator eval;

    std::vector<std::vector<int>> routes = {
        {0, 1, 2, 3, 4, 5},
        {},  // empty
        {}   // empty
    };

    Solution sol = warm_start(routes, data, eval);

    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.num_used_vehicles() == 1);
}

TEST_CASE("warm_start — cost is positive and consistent", "[warm_start]") {
    auto data = make_small_cvrp();
    CostEvaluator eval;

    std::vector<std::vector<int>> routes = {{0, 1, 2}, {3, 4, 5}};

    Solution sol = warm_start(routes, data, eval);

    int64_t cost = sol.cost(eval);
    CHECK(cost > 0);

    // Distance should be positive.
    CHECK(sol.total_distance() > 0);
}

TEST_CASE("warm_start — optional clients can be omitted", "[warm_start]") {
    auto data = make_optional_cvrp();
    CostEvaluator eval;

    // Only assign the required clients (0 and 1), skip optional (2 and 3).
    std::vector<std::vector<int>> routes = {{0, 1}};

    Solution sol = warm_start(routes, data, eval);

    CHECK(sol.num_unassigned() == 2);  // clients 2, 3 unassigned
    CHECK(sol.is_assigned(0));
    CHECK(sol.is_assigned(1));
    CHECK_FALSE(sol.is_assigned(2));
    CHECK_FALSE(sol.is_assigned(3));
}

TEST_CASE("warm_start — optional clients can be included", "[warm_start]") {
    auto data = make_optional_cvrp();
    CostEvaluator eval;

    // Assign all clients including optional ones.
    std::vector<std::vector<int>> routes = {{0, 1, 2, 3}};

    Solution sol = warm_start(routes, data, eval);

    CHECK(sol.num_unassigned() == 0);
}

// ===========================================================================
//  warm_start validation error tests
// ===========================================================================

TEST_CASE("warm_start — too many routes throws", "[warm_start]") {
    auto data = make_small_cvrp();  // 3 vehicles
    CostEvaluator eval;

    std::vector<std::vector<int>> routes = {
        {0}, {1}, {2}, {3}  // 4 routes > 3 vehicles
    };

    CHECK_THROWS_AS(warm_start(routes, data, eval), std::invalid_argument);
}

TEST_CASE("warm_start — out-of-range client throws", "[warm_start]") {
    auto data = make_small_cvrp();  // 6 clients (0-5)
    CostEvaluator eval;

    std::vector<std::vector<int>> routes = {
        {0, 1, 2, 3, 4, 99}  // client 99 does not exist
    };

    CHECK_THROWS_AS(warm_start(routes, data, eval), std::invalid_argument);
}

TEST_CASE("warm_start — negative client index throws", "[warm_start]") {
    auto data = make_small_cvrp();
    CostEvaluator eval;

    std::vector<std::vector<int>> routes = {{0, 1, -1}};

    CHECK_THROWS_AS(warm_start(routes, data, eval), std::invalid_argument);
}

TEST_CASE("warm_start — duplicate client throws", "[warm_start]") {
    auto data = make_small_cvrp();
    CostEvaluator eval;

    // Client 2 appears twice.
    std::vector<std::vector<int>> routes = {{0, 1, 2}, {2, 3, 4, 5}};

    CHECK_THROWS_AS(warm_start(routes, data, eval), std::invalid_argument);
}

TEST_CASE("warm_start — missing required client throws", "[warm_start]") {
    auto data = make_small_cvrp();  // all required
    CostEvaluator eval;

    // Only 5 out of 6 clients assigned.
    std::vector<std::vector<int>> routes = {
        {0, 1, 2}, {3, 4}  // client 5 missing
    };

    CHECK_THROWS_AS(warm_start(routes, data, eval), std::invalid_argument);
}

TEST_CASE("warm_start — empty routes vector assigns nothing", "[warm_start]") {
    auto data = make_optional_cvrp();
    CostEvaluator eval;

    // No routes but all required clients must be present => should throw
    // because clients 0 and 1 are required.
    std::vector<std::vector<int>> routes = {};

    CHECK_THROWS_AS(warm_start(routes, data, eval), std::invalid_argument);
}
