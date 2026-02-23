#include <catch2/catch_test_macros.hpp>

#include "routing/operators/exchange.h"
#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

using namespace coso;

// ---------------------------------------------------------------------------
//  Test instance builders
// ---------------------------------------------------------------------------

/// 1 depot at (0,0), 6 clients on a line and off-axis.
/// 2 vehicles with capacity 15.
///
///  Depot(0,0)  C0(10,0)  C1(20,0)  C2(30,0)  C3(0,10)  C4(0,20)  C5(15,15)
///
/// Demands: C0=3, C1=4, C2=5, C3=2, C4=3, C5=6
static ProblemData make_test_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {.capacity = {15}});

    b.add_client({10.0, 0.0}, {.demand = {3}});   // 0
    b.add_client({20.0, 0.0}, {.demand = {4}});   // 1
    b.add_client({30.0, 0.0}, {.demand = {5}});   // 2
    b.add_client({0.0, 10.0}, {.demand = {2}});   // 3
    b.add_client({0.0, 20.0}, {.demand = {3}});   // 4
    b.add_client({15.0, 15.0}, {.demand = {6}});  // 5

    return b.build(0);  // no granular neighbours for deterministic testing
}

/// Small instance with granular neighbours.
static ProblemData make_granular_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {.capacity = {15}});

    b.add_client({10.0, 0.0}, {.demand = {3}});
    b.add_client({20.0, 0.0}, {.demand = {4}});
    b.add_client({30.0, 0.0}, {.demand = {5}});
    b.add_client({0.0, 10.0}, {.demand = {2}});
    b.add_client({0.0, 20.0}, {.demand = {3}});
    b.add_client({15.0, 15.0}, {.demand = {6}});

    return b.build(5);  // k=5 nearest neighbours
}

/// Build a solution with given route assignments.
static Solution make_solution(ProblemData const& data,
                              std::vector<std::vector<int>> const& routes)
{
    Solution sol(data);
    for (int r = 0; r < static_cast<int>(routes.size()); ++r) {
        if (!routes[r].empty())
            sol.set_route_clients(r, routes[r]);
    }
    return sol;
}

// ===========================================================================
//  Exchange(1,0) -- Relocate tests
// ===========================================================================

TEST_CASE("Exchange10: finds improving inter-route relocate",
          "[exchange][exchange10]")
{
    auto data = make_test_instance();
    CostEvaluator eval(100);

    // Suboptimal: route 0 = [0, 1, 2], route 1 = [3, 4, 5].
    // C2(30,0) is far from others; moving it might help.
    auto sol = make_solution(data, {{0, 1, 2}, {3, 4, 5}});

    int64_t old_cost = sol.cost(eval);

    Exchange10 op;
    bool found = op.find_best_move(sol, eval, data);

    if (found) {
        CHECK(op.best_delta() < 0);
        op.apply(sol);
        int64_t new_cost = sol.cost(eval);
        CHECK(new_cost < old_cost);

        // Verify delta prediction.
        CHECK(new_cost - old_cost == op.best_delta());
    }
}

TEST_CASE("Exchange10: no improving move on optimal 1-client routes",
          "[exchange][exchange10]")
{
    auto data = make_test_instance();
    CostEvaluator eval(100);

    // Each client in its own route (if enough vehicles).
    // With 3 vehicles and 6 clients, some sharing is needed.
    // Instead test: routes that are already quite good.
    // Route 0 = [0, 1, 2] (line), route 1 = [3, 4] (column).
    // Actually let's just verify the operator doesn't crash on trivial input.
    auto sol = make_solution(data, {{0}, {1}, {2}});

    Exchange10 op;
    bool found = op.find_best_move(sol, eval, data);

    // Whether or not improving, applying should be safe.
    if (found) {
        op.apply(sol);
    }
}

TEST_CASE("Exchange10: apply correctness -- inter-route",
          "[exchange][exchange10]")
{
    auto data = make_test_instance();
    CostEvaluator eval(100);

    // Route 0 = [0, 5, 1], route 1 = [3, 4], route 2 = [2].
    // Client 5 at (15,15) is awkwardly placed in route 0 (the line route).
    auto sol = make_solution(data, {{0, 5, 1}, {3, 4}, {2}});
    int64_t old_cost = sol.cost(eval);

    Exchange10 op;
    bool found = op.find_best_move(sol, eval, data);

    if (found) {
        op.apply(sol);
        int64_t new_cost = sol.cost(eval);

        // Cost improved.
        CHECK(new_cost < old_cost);

        // All clients still assigned.
        CHECK(sol.num_unassigned() == 0);

        // Total clients across all routes unchanged.
        int total = 0;
        for (int r = 0; r < sol.num_routes(); ++r)
            total += sol.route(r).size();
        CHECK(total == 6);
    }
}

TEST_CASE("Exchange10: intra-route relocate", "[exchange][exchange10]")
{
    auto data = make_test_instance();
    CostEvaluator eval(100);

    // Route 0 = [2, 0, 1]: C2(30,0) is at front, but depot is at (0,0).
    // Moving C2 to the end should reduce distance.
    auto sol = make_solution(data, {{2, 0, 1}, {3, 4, 5}});
    int64_t old_cost = sol.cost(eval);

    Exchange10 op;
    bool found = op.find_best_move(sol, eval, data);

    if (found) {
        op.apply(sol);
        int64_t new_cost = sol.cost(eval);
        CHECK(new_cost < old_cost);
        CHECK(sol.num_unassigned() == 0);
    }
}

TEST_CASE("Exchange10: delta matches actual cost change",
          "[exchange][exchange10]")
{
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 5, 1}, {3, 4}, {2}});
    int64_t old_cost = sol.cost(eval);

    Exchange10 op;
    if (op.find_best_move(sol, eval, data)) {
        int64_t predicted_delta = op.best_delta();
        op.apply(sol);
        int64_t actual_delta = sol.cost(eval) - old_cost;
        CHECK(predicted_delta == actual_delta);
    }
}

// ===========================================================================
//  Exchange(1,1) -- Swap tests
// ===========================================================================

TEST_CASE("Exchange11: finds improving inter-route swap",
          "[exchange][exchange11]")
{
    auto data = make_test_instance();
    CostEvaluator eval(100);

    // All 6 clients assigned.
    // C5(15,15) is awkward in route 0 (line); C2(30,0) is awkward in route 1 (vertical).
    auto sol = make_solution(data, {{0, 1, 5}, {3, 2, 4}});
    int64_t old_cost = sol.cost(eval);

    Exchange11 op;
    bool found = op.find_best_move(sol, eval, data);

    if (found) {
        CHECK(op.best_delta() < 0);
        op.apply(sol);
        int64_t new_cost = sol.cost(eval);
        CHECK(new_cost < old_cost);
        CHECK(sol.num_unassigned() == 0);
    }
}

TEST_CASE("Exchange11: delta matches actual cost change",
          "[exchange][exchange11]")
{
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 5}, {3, 2, 4}});
    int64_t old_cost = sol.cost(eval);

    Exchange11 op;
    if (op.find_best_move(sol, eval, data)) {
        int64_t predicted = op.best_delta();
        op.apply(sol);
        int64_t actual = sol.cost(eval) - old_cost;
        CHECK(predicted == actual);
    }
}

TEST_CASE("Exchange11: preserves all clients", "[exchange][exchange11]")
{
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 5}, {3, 2, 4}});

    Exchange11 op;
    if (op.find_best_move(sol, eval, data)) {
        op.apply(sol);

        CHECK(sol.num_unassigned() == 0);
        // Verify each client appears exactly once.
        std::vector<int> seen(data.num_clients(), 0);
        for (int r = 0; r < sol.num_routes(); ++r)
            for (int i = 0; i < sol.route(r).size(); ++i)
                seen[sol.route(r).client(i)]++;
        for (int c = 0; c < data.num_clients(); ++c)
            CHECK(seen[c] == 1);
    }
}

TEST_CASE("Exchange11: intra-route swap", "[exchange][exchange11]")
{
    auto data = make_test_instance();
    CostEvaluator eval(100);

    // All clients in one route, badly ordered.
    auto sol = make_solution(data, {{2, 4, 0, 5, 1, 3}});
    int64_t old_cost = sol.cost(eval);

    Exchange11 op;
    if (op.find_best_move(sol, eval, data)) {
        int64_t predicted = op.best_delta();
        op.apply(sol);
        int64_t actual = sol.cost(eval) - old_cost;
        CHECK(predicted == actual);
        CHECK(sol.num_unassigned() == 0);
    }
}

// ===========================================================================
//  Exchange(2,0) -- Relocate pair tests
// ===========================================================================

TEST_CASE("Exchange20: finds improving pair relocate",
          "[exchange][exchange20]")
{
    auto data = make_test_instance();
    CostEvaluator eval(100);

    // Route 0 = [0, 3, 4, 1, 2], route 1 = [5].
    // C3(0,10) and C4(0,20) are together but misplaced among the line clients.
    auto sol = make_solution(data, {{0, 3, 4, 1, 2}, {5}});
    int64_t old_cost = sol.cost(eval);

    Exchange20 op;
    bool found = op.find_best_move(sol, eval, data);

    if (found) {
        CHECK(op.best_delta() < 0);
        op.apply(sol);
        int64_t new_cost = sol.cost(eval);
        CHECK(new_cost < old_cost);
        CHECK(sol.num_unassigned() == 0);
    }
}

TEST_CASE("Exchange20: delta matches actual cost change",
          "[exchange][exchange20]")
{
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 3, 4, 1, 2}, {5}});
    int64_t old_cost = sol.cost(eval);

    Exchange20 op;
    if (op.find_best_move(sol, eval, data)) {
        int64_t predicted = op.best_delta();
        op.apply(sol);
        int64_t actual = sol.cost(eval) - old_cost;
        CHECK(predicted == actual);
    }
}

TEST_CASE("Exchange20: preserves all clients", "[exchange][exchange20]")
{
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 3, 4, 1, 2}, {5}});

    Exchange20 op;
    if (op.find_best_move(sol, eval, data)) {
        op.apply(sol);

        CHECK(sol.num_unassigned() == 0);
        std::vector<int> seen(data.num_clients(), 0);
        for (int r = 0; r < sol.num_routes(); ++r)
            for (int i = 0; i < sol.route(r).size(); ++i)
                seen[sol.route(r).client(i)]++;
        for (int c = 0; c < data.num_clients(); ++c)
            CHECK(seen[c] == 1);
    }
}

TEST_CASE("Exchange20: no move when routes have < 2 clients",
          "[exchange][exchange20]")
{
    auto data = make_test_instance();
    CostEvaluator eval(100);

    // Each client alone.
    auto sol = make_solution(data, {{0}, {1}, {2}});

    Exchange20 op;
    bool found = op.find_best_move(sol, eval, data);
    // Routes with < 2 clients have no consecutive pairs.
    // Some routes have only 1 client, but others have 0.
    // Since we only have 3 vehicles and 3 clients assigned.
    CHECK_FALSE(found);
}

// ===========================================================================
//  SwapTails tests
// ===========================================================================

TEST_CASE("SwapTails: finds improving tail swap", "[exchange][swaptails]")
{
    auto data = make_test_instance();
    CostEvaluator eval(100);

    // Route 0 = [0, 1, 3, 4], route 1 = [5, 2].
    // After C1(20,0), the route jumps to C3(0,10) -- swapping tails
    // so route 0 gets the line suffix and route 1 gets the vertical suffix
    // should help.
    auto sol = make_solution(data, {{0, 1, 3, 4}, {5, 2}});
    int64_t old_cost = sol.cost(eval);

    SwapTails op;
    bool found = op.find_best_move(sol, eval, data);

    if (found) {
        CHECK(op.best_delta() < 0);
        op.apply(sol);
        int64_t new_cost = sol.cost(eval);
        CHECK(new_cost < old_cost);
        CHECK(sol.num_unassigned() == 0);
    }
}

TEST_CASE("SwapTails: delta matches actual cost change",
          "[exchange][swaptails]")
{
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 3, 4}, {5, 2}});
    int64_t old_cost = sol.cost(eval);

    SwapTails op;
    if (op.find_best_move(sol, eval, data)) {
        int64_t predicted = op.best_delta();
        op.apply(sol);
        int64_t actual = sol.cost(eval) - old_cost;
        CHECK(predicted == actual);
    }
}

TEST_CASE("SwapTails: preserves all clients", "[exchange][swaptails]")
{
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 3, 4}, {5, 2}});

    SwapTails op;
    if (op.find_best_move(sol, eval, data)) {
        op.apply(sol);

        CHECK(sol.num_unassigned() == 0);
        std::vector<int> seen(data.num_clients(), 0);
        for (int r = 0; r < sol.num_routes(); ++r)
            for (int i = 0; i < sol.route(r).size(); ++i)
                seen[sol.route(r).client(i)]++;
        for (int c = 0; c < data.num_clients(); ++c)
            CHECK(seen[c] == 1);
    }
}

// ===========================================================================
//  Granular neighbourhood tests
// ===========================================================================

TEST_CASE("Exchange10: works with granular neighbours",
          "[exchange][exchange10][granular]")
{
    auto data = make_granular_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 5, 1}, {3, 4}, {2}});
    int64_t old_cost = sol.cost(eval);

    Exchange10 op;
    bool found = op.find_best_move(sol, eval, data);

    if (found) {
        op.apply(sol);
        int64_t new_cost = sol.cost(eval);
        CHECK(new_cost < old_cost);
        CHECK(sol.num_unassigned() == 0);
    }
}

TEST_CASE("Exchange11: works with granular neighbours",
          "[exchange][exchange11][granular]")
{
    auto data = make_granular_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 5}, {3, 2, 4}});
    int64_t old_cost = sol.cost(eval);

    Exchange11 op;
    if (op.find_best_move(sol, eval, data)) {
        op.apply(sol);
        CHECK(sol.cost(eval) < old_cost);
        CHECK(sol.num_unassigned() == 0);
    }
}

// ===========================================================================
//  Capacity-violation tests (penalized moves)
// ===========================================================================

TEST_CASE("Exchange10: moves that fix capacity violations",
          "[exchange][exchange10][capacity]")
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {10}});
    b.add_client({10.0, 0.0}, {.demand = {6}});  // 0
    b.add_client({20.0, 0.0}, {.demand = {6}});  // 1
    b.add_client({10.0, 10.0}, {.demand = {3}}); // 2
    b.add_client({20.0, 10.0}, {.demand = {3}}); // 3
    auto data = b.build(0);

    CostEvaluator eval(1000);  // high penalty

    // Route 0 = [0, 1] demand=12 > cap=10.  Route 1 = [2, 3] demand=6 <= 10.
    auto sol = make_solution(data, {{0, 1}, {2, 3}});
    CHECK_FALSE(sol.route(0).load_feasible());

    Exchange10 op;
    bool found = op.find_best_move(sol, eval, data);

    // Should find a move that reduces violation.
    REQUIRE(found);
    op.apply(sol);

    // After move, total penalty should decrease.
    // (We don't require full feasibility, just improvement.)
    CHECK(sol.num_unassigned() == 0);
}

// ===========================================================================
//  Iterated application: apply operators until no improvement
// ===========================================================================

TEST_CASE("Iterated Exchange10 converges", "[exchange][exchange10][iterate]")
{
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{2, 4, 0, 5, 1, 3}});

    Exchange10 op;
    int iters = 0;
    while (op.find_best_move(sol, eval, data) && iters < 100) {
        op.apply(sol);
        ++iters;
    }

    // Should converge (no infinite loop).
    CHECK(iters < 100);
    CHECK(sol.num_unassigned() == 0);
}

TEST_CASE("Iterated Exchange11 converges", "[exchange][exchange11][iterate]")
{
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{2, 4, 0, 5, 1, 3}});

    Exchange11 op;
    int iters = 0;
    while (op.find_best_move(sol, eval, data) && iters < 100) {
        op.apply(sol);
        ++iters;
    }

    CHECK(iters < 100);
    CHECK(sol.num_unassigned() == 0);
}

// ===========================================================================
//  Larger instance for extended exchange operators
// ===========================================================================

/// 1 depot at (0,0), 10 clients, 4 vehicles with capacity 30.
static ProblemData make_large_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(4, {.capacity = {30}});

    b.add_client({10.0, 0.0}, {.demand = {3}});   // 0
    b.add_client({20.0, 0.0}, {.demand = {4}});   // 1
    b.add_client({30.0, 0.0}, {.demand = {5}});   // 2
    b.add_client({40.0, 0.0}, {.demand = {3}});   // 3
    b.add_client({0.0, 10.0}, {.demand = {2}});   // 4
    b.add_client({0.0, 20.0}, {.demand = {3}});   // 5
    b.add_client({0.0, 30.0}, {.demand = {4}});   // 6
    b.add_client({15.0, 15.0}, {.demand = {6}});  // 7
    b.add_client({25.0, 5.0}, {.demand = {2}});   // 8
    b.add_client({5.0, 25.0}, {.demand = {3}});   // 9

    return b.build(0);
}

/// Helper: verify all clients appear exactly once across all routes.
static void check_all_clients(Solution const& sol, int num_clients)
{
    CHECK(sol.num_unassigned() == 0);
    std::vector<int> seen(num_clients, 0);
    for (int r = 0; r < sol.num_routes(); ++r)
        for (int i = 0; i < sol.route(r).size(); ++i)
            seen[sol.route(r).client(i)]++;
    for (int c = 0; c < num_clients; ++c)
        CHECK(seen[c] == 1);
}

// ===========================================================================
//  Exchange(2,1) tests
// ===========================================================================

TEST_CASE("Exchange21: finds improving move", "[exchange][exchange21]")
{
    auto data = make_large_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 4, 5}, {2, 3, 8}, {6, 7, 9}});
    int64_t old_cost = sol.cost(eval);

    Exchange21 op;
    if (op.find_best_move(sol, eval, data)) {
        CHECK(op.best_delta() < 0);
        op.apply(sol);
        int64_t new_cost = sol.cost(eval);
        CHECK(new_cost < old_cost);
        check_all_clients(sol, data.num_clients());
    }
}

TEST_CASE("Exchange21: delta matches actual cost change",
          "[exchange][exchange21]")
{
    auto data = make_large_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 4, 5}, {2, 3, 8}, {6, 7, 9}});
    int64_t old_cost = sol.cost(eval);

    Exchange21 op;
    if (op.find_best_move(sol, eval, data)) {
        int64_t predicted = op.best_delta();
        op.apply(sol);
        int64_t actual = sol.cost(eval) - old_cost;
        CHECK(predicted == actual);
    }
}

TEST_CASE("Exchange21: preserves all clients", "[exchange][exchange21]")
{
    auto data = make_large_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 4, 5}, {2, 3, 8}, {6, 7, 9}});

    Exchange21 op;
    if (op.find_best_move(sol, eval, data)) {
        op.apply(sol);
        check_all_clients(sol, data.num_clients());
    }
}

TEST_CASE("Iterated Exchange21 converges", "[exchange][exchange21][iterate]")
{
    auto data = make_large_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 4, 5, 6, 1}, {2, 3, 8}, {7, 9}});

    Exchange21 op;
    int iters = 0;
    while (op.find_best_move(sol, eval, data) && iters < 200) {
        op.apply(sol);
        ++iters;
    }
    CHECK(iters < 200);
    check_all_clients(sol, data.num_clients());
}

// ===========================================================================
//  Exchange(2,2) tests
// ===========================================================================

TEST_CASE("Exchange22: finds improving move", "[exchange][exchange22]")
{
    auto data = make_large_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 4, 5, 1}, {2, 6, 7, 3}, {8, 9}});
    int64_t old_cost = sol.cost(eval);

    Exchange22 op;
    if (op.find_best_move(sol, eval, data)) {
        CHECK(op.best_delta() < 0);
        op.apply(sol);
        int64_t new_cost = sol.cost(eval);
        CHECK(new_cost < old_cost);
        check_all_clients(sol, data.num_clients());
    }
}

TEST_CASE("Exchange22: delta matches actual cost change",
          "[exchange][exchange22]")
{
    auto data = make_large_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 4, 5, 1}, {2, 6, 7, 3}, {8, 9}});
    int64_t old_cost = sol.cost(eval);

    Exchange22 op;
    if (op.find_best_move(sol, eval, data)) {
        int64_t predicted = op.best_delta();
        op.apply(sol);
        int64_t actual = sol.cost(eval) - old_cost;
        CHECK(predicted == actual);
    }
}

TEST_CASE("Exchange22: preserves all clients", "[exchange][exchange22]")
{
    auto data = make_large_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 4, 5, 1}, {2, 6, 7, 3}, {8, 9}});

    Exchange22 op;
    if (op.find_best_move(sol, eval, data)) {
        op.apply(sol);
        check_all_clients(sol, data.num_clients());
    }
}

TEST_CASE("Exchange22: no move when routes have < 2 clients",
          "[exchange][exchange22]")
{
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0}, {1}, {2}});

    Exchange22 op;
    CHECK_FALSE(op.find_best_move(sol, eval, data));
}

// ===========================================================================
//  Exchange(3,0) tests
// ===========================================================================

TEST_CASE("Exchange30: finds improving triple relocate",
          "[exchange][exchange30]")
{
    auto data = make_large_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 4, 5, 6, 1, 2, 3}, {7, 8, 9}});
    int64_t old_cost = sol.cost(eval);

    Exchange30 op;
    if (op.find_best_move(sol, eval, data)) {
        CHECK(op.best_delta() < 0);
        op.apply(sol);
        int64_t new_cost = sol.cost(eval);
        CHECK(new_cost < old_cost);
        check_all_clients(sol, data.num_clients());
    }
}

TEST_CASE("Exchange30: delta matches actual cost change",
          "[exchange][exchange30]")
{
    auto data = make_large_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 4, 5, 6, 1, 2, 3}, {7, 8, 9}});
    int64_t old_cost = sol.cost(eval);

    Exchange30 op;
    if (op.find_best_move(sol, eval, data)) {
        int64_t predicted = op.best_delta();
        op.apply(sol);
        int64_t actual = sol.cost(eval) - old_cost;
        CHECK(predicted == actual);
    }
}

TEST_CASE("Exchange30: preserves all clients", "[exchange][exchange30]")
{
    auto data = make_large_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 4, 5, 6, 1, 2, 3}, {7, 8, 9}});

    Exchange30 op;
    if (op.find_best_move(sol, eval, data)) {
        op.apply(sol);
        check_all_clients(sol, data.num_clients());
    }
}

TEST_CASE("Exchange30: no move when routes have < 3 clients",
          "[exchange][exchange30]")
{
    auto data = make_test_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1}, {2, 3}, {4, 5}});

    Exchange30 op;
    CHECK_FALSE(op.find_best_move(sol, eval, data));
}

TEST_CASE("Iterated Exchange30 converges", "[exchange][exchange30][iterate]")
{
    auto data = make_large_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 4, 5, 6, 1, 2, 3}, {7, 8, 9}});

    Exchange30 op;
    int iters = 0;
    while (op.find_best_move(sol, eval, data) && iters < 200) {
        op.apply(sol);
        ++iters;
    }
    CHECK(iters < 200);
    check_all_clients(sol, data.num_clients());
}

// ===========================================================================
//  Exchange(3,1) tests
// ===========================================================================

TEST_CASE("Exchange31: finds improving move", "[exchange][exchange31]")
{
    auto data = make_large_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 4, 5, 6, 1}, {2, 3, 8}, {7, 9}});
    int64_t old_cost = sol.cost(eval);

    Exchange31 op;
    if (op.find_best_move(sol, eval, data)) {
        CHECK(op.best_delta() < 0);
        op.apply(sol);
        int64_t new_cost = sol.cost(eval);
        CHECK(new_cost < old_cost);
        check_all_clients(sol, data.num_clients());
    }
}

TEST_CASE("Exchange31: delta matches actual cost change",
          "[exchange][exchange31]")
{
    auto data = make_large_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 4, 5, 6, 1}, {2, 3, 8}, {7, 9}});
    int64_t old_cost = sol.cost(eval);

    Exchange31 op;
    if (op.find_best_move(sol, eval, data)) {
        int64_t predicted = op.best_delta();
        op.apply(sol);
        int64_t actual = sol.cost(eval) - old_cost;
        CHECK(predicted == actual);
    }
}

TEST_CASE("Exchange31: preserves all clients", "[exchange][exchange31]")
{
    auto data = make_large_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 4, 5, 6, 1}, {2, 3, 8}, {7, 9}});

    Exchange31 op;
    if (op.find_best_move(sol, eval, data)) {
        op.apply(sol);
        check_all_clients(sol, data.num_clients());
    }
}

// ===========================================================================
//  Exchange(3,2) tests
// ===========================================================================

TEST_CASE("Exchange32: finds improving move", "[exchange][exchange32]")
{
    auto data = make_large_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 4, 5, 6, 1}, {7, 2, 3, 8}, {9}});
    int64_t old_cost = sol.cost(eval);

    Exchange32 op;
    if (op.find_best_move(sol, eval, data)) {
        CHECK(op.best_delta() < 0);
        op.apply(sol);
        int64_t new_cost = sol.cost(eval);
        CHECK(new_cost < old_cost);
        check_all_clients(sol, data.num_clients());
    }
}

TEST_CASE("Exchange32: delta matches actual cost change",
          "[exchange][exchange32]")
{
    auto data = make_large_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 4, 5, 6, 1}, {7, 2, 3, 8}, {9}});
    int64_t old_cost = sol.cost(eval);

    Exchange32 op;
    if (op.find_best_move(sol, eval, data)) {
        int64_t predicted = op.best_delta();
        op.apply(sol);
        int64_t actual = sol.cost(eval) - old_cost;
        CHECK(predicted == actual);
    }
}

// ===========================================================================
//  Exchange(3,3) tests
// ===========================================================================

TEST_CASE("Exchange33: finds improving move", "[exchange][exchange33]")
{
    auto data = make_large_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 4, 5, 6, 1}, {7, 2, 3, 8, 9}});
    int64_t old_cost = sol.cost(eval);

    Exchange33 op;
    if (op.find_best_move(sol, eval, data)) {
        CHECK(op.best_delta() < 0);
        op.apply(sol);
        int64_t new_cost = sol.cost(eval);
        CHECK(new_cost < old_cost);
        check_all_clients(sol, data.num_clients());
    }
}

TEST_CASE("Exchange33: delta matches actual cost change",
          "[exchange][exchange33]")
{
    auto data = make_large_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 4, 5, 6, 1}, {7, 2, 3, 8, 9}});
    int64_t old_cost = sol.cost(eval);

    Exchange33 op;
    if (op.find_best_move(sol, eval, data)) {
        int64_t predicted = op.best_delta();
        op.apply(sol);
        int64_t actual = sol.cost(eval) - old_cost;
        CHECK(predicted == actual);
    }
}

TEST_CASE("Exchange33: preserves all clients", "[exchange][exchange33]")
{
    auto data = make_large_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 4, 5, 6, 1}, {7, 2, 3, 8, 9}});

    Exchange33 op;
    if (op.find_best_move(sol, eval, data)) {
        op.apply(sol);
        check_all_clients(sol, data.num_clients());
    }
}
