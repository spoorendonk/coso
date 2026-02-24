#include <catch2/catch_test_macros.hpp>

#include "routing/operators/insert_optional.h"
#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

using namespace coso;

// ---------------------------------------------------------------------------
//  Test instance builders
// ---------------------------------------------------------------------------

/// 1 depot at (0,0), 4 required clients + 2 optional clients with prizes.
/// 2 vehicles with capacity 20.
///
///  Depot(0,0)  C0(10,0)  C1(20,0)  C2(0,10)  C3(0,20)
///  Optional: C4(5,5) prize=100, C5(15,15) prize=5
///
/// C4 has a high prize (worth inserting), C5 has a low prize (may not be worth it).
static ProblemData make_optional_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {20}});

    // Required clients.
    b.add_client({10.0, 0.0}, {.demand = {3}});   // 0 - required
    b.add_client({20.0, 0.0}, {.demand = {4}});   // 1 - required
    b.add_client({0.0, 10.0}, {.demand = {2}});   // 2 - required
    b.add_client({0.0, 20.0}, {.demand = {3}});   // 3 - required

    // Optional clients.
    b.add_client({5.0, 5.0}, {.demand = {1}, .prize = 100, .required = false});   // 4 - high prize
    b.add_client({15.0, 15.0}, {.demand = {1}, .prize = 5, .required = false});   // 5 - low prize

    return b.build(0);
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
//  InsertOptional tests
// ===========================================================================

TEST_CASE("InsertOptional: inserts high-prize optional client",
          "[insert_optional]")
{
    auto data = make_optional_instance();
    CostEvaluator eval(100);

    // Only required clients served; optional clients 4,5 are unassigned.
    auto sol = make_solution(data, {{0, 1}, {2, 3}});
    CHECK(sol.num_unassigned() == 2);

    int64_t old_cost = sol.cost(eval);

    InsertOptional op;
    bool found = op.find_best_move(sol, eval, data);

    // C4 has prize=100, so insertion should be profitable.
    REQUIRE(found);
    CHECK(op.best_delta() < 0);

    op.apply(sol);
    int64_t new_cost = sol.cost(eval);
    CHECK(new_cost < old_cost);
    CHECK(new_cost - old_cost == op.best_delta());

    // C4 should now be assigned (one fewer unassigned).
    CHECK(sol.num_unassigned() == 1);
}

TEST_CASE("InsertOptional: does not insert when prize is too low",
          "[insert_optional]")
{
    auto data = make_optional_instance();
    CostEvaluator eval(100);

    // Serve required clients and the high-prize optional client 4.
    // Only client 5 (prize=5) remains unassigned.
    auto sol = make_solution(data, {{0, 1, 4}, {2, 3}});
    CHECK(sol.num_unassigned() == 1);
    CHECK_FALSE(sol.is_assigned(5));

    InsertOptional op;
    bool found = op.find_best_move(sol, eval, data);

    // Prize of 5 is very low compared to the insertion distance cost
    // for C5(15,15), so no improving insertion should exist.
    CHECK_FALSE(found);
}

TEST_CASE("InsertOptional: no move when no unassigned optional clients",
          "[insert_optional]")
{
    auto data = make_optional_instance();
    CostEvaluator eval(100);

    // All clients served.
    auto sol = make_solution(data, {{0, 1, 4}, {2, 3, 5}});
    CHECK(sol.num_unassigned() == 0);

    InsertOptional op;
    CHECK_FALSE(op.find_best_move(sol, eval, data));
}

TEST_CASE("InsertOptional: no move when only required clients unassigned",
          "[insert_optional]")
{
    // Instance with no optional clients at all.
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {20}});
    b.add_client({10.0, 0.0}, {.demand = {3}});
    b.add_client({20.0, 0.0}, {.demand = {4}});
    auto data = b.build(0);

    CostEvaluator eval(100);

    // Leave client 1 unassigned (but it's required, so InsertOptional ignores it).
    auto sol = make_solution(data, {{0}});
    CHECK(sol.num_unassigned() == 1);

    InsertOptional op;
    CHECK_FALSE(op.find_best_move(sol, eval, data));
}

TEST_CASE("InsertOptional: delta matches actual cost change",
          "[insert_optional]")
{
    auto data = make_optional_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1}, {2, 3}});
    int64_t old_cost = sol.cost(eval);

    InsertOptional op;
    if (op.find_best_move(sol, eval, data)) {
        int64_t predicted = op.best_delta();
        op.apply(sol);
        int64_t actual = sol.cost(eval) - old_cost;
        CHECK(predicted == actual);
    }
}

TEST_CASE("InsertOptional: iterates until no more insertions",
          "[insert_optional][iterate]")
{
    auto data = make_optional_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1}, {2, 3}});

    InsertOptional op;
    int iters = 0;
    while (op.find_best_move(sol, eval, data) && iters < 100) {
        op.apply(sol);
        ++iters;
    }

    CHECK(iters < 100);
    // At minimum the high-prize client should have been inserted.
    CHECK(sol.is_assigned(4));
}

TEST_CASE("InsertOptional: chooses best position across routes",
          "[insert_optional]")
{
    auto data = make_optional_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1}, {2, 3}});
    int64_t old_cost = sol.cost(eval);

    InsertOptional op;
    REQUIRE(op.find_best_move(sol, eval, data));

    // Verify this is indeed the best possible insertion by trying all.
    int64_t best_delta = 0;
    for (int c : sol.unassigned()) {
        if (data.client(c).required) continue;
        for (int r = 0; r < sol.num_routes(); ++r) {
            auto const& route = sol.route(r);
            for (int p = 0; p <= route.size(); ++p) {
                int64_t d = eval.eval_insert_cost(route, p, c);
                if (d < best_delta) best_delta = d;
            }
        }
    }
    CHECK(op.best_delta() == best_delta);
}

// ===========================================================================
//  RemoveOptional tests
// ===========================================================================

TEST_CASE("RemoveOptional: removes low-prize optional client",
          "[remove_optional]")
{
    auto data = make_optional_instance();
    CostEvaluator eval(100);

    // Serve all clients including both optional ones.
    // C5(15,15) with prize=5 is probably not worth the detour.
    auto sol = make_solution(data, {{0, 1}, {2, 5, 3}});
    int64_t old_cost = sol.cost(eval);

    RemoveOptional op;
    bool found = op.find_best_move(sol, eval, data);

    REQUIRE(found);
    CHECK(op.best_delta() < 0);

    op.apply(sol);
    int64_t new_cost = sol.cost(eval);
    CHECK(new_cost < old_cost);
    CHECK(new_cost - old_cost == op.best_delta());
}

TEST_CASE("RemoveOptional: does not remove high-prize client",
          "[remove_optional]")
{
    auto data = make_optional_instance();
    CostEvaluator eval(100);

    // Only the high-prize optional client 4 is served.
    // It is close to the route and has prize=100, so removal is costly.
    auto sol = make_solution(data, {{0, 4, 1}, {2, 3}});
    int64_t old_cost = sol.cost(eval);

    RemoveOptional op;
    bool found = op.find_best_move(sol, eval, data);

    // Removing C4 would lose prize=100 but only save a small detour,
    // so no improving removal should exist.
    CHECK_FALSE(found);
}

TEST_CASE("RemoveOptional: no move when no optional clients served",
          "[remove_optional]")
{
    auto data = make_optional_instance();
    CostEvaluator eval(100);

    // Only required clients served.
    auto sol = make_solution(data, {{0, 1}, {2, 3}});

    RemoveOptional op;
    CHECK_FALSE(op.find_best_move(sol, eval, data));
}

TEST_CASE("RemoveOptional: does not remove required clients",
          "[remove_optional]")
{
    // Instance with only required clients.
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {20}});
    b.add_client({10.0, 0.0}, {.demand = {3}});
    b.add_client({20.0, 0.0}, {.demand = {4}});
    auto data = b.build(0);

    CostEvaluator eval(100);
    auto sol = make_solution(data, {{0, 1}});

    RemoveOptional op;
    CHECK_FALSE(op.find_best_move(sol, eval, data));
}

TEST_CASE("RemoveOptional: delta matches actual cost change",
          "[remove_optional]")
{
    auto data = make_optional_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1}, {2, 5, 3}});
    int64_t old_cost = sol.cost(eval);

    RemoveOptional op;
    if (op.find_best_move(sol, eval, data)) {
        int64_t predicted = op.best_delta();
        op.apply(sol);
        int64_t actual = sol.cost(eval) - old_cost;
        CHECK(predicted == actual);
    }
}

TEST_CASE("RemoveOptional: iterates until no more removals",
          "[remove_optional][iterate]")
{
    auto data = make_optional_instance();
    CostEvaluator eval(100);

    // Serve both optional clients.
    auto sol = make_solution(data, {{0, 4, 1}, {2, 5, 3}});

    RemoveOptional op;
    int iters = 0;
    while (op.find_best_move(sol, eval, data) && iters < 100) {
        op.apply(sol);
        ++iters;
    }

    CHECK(iters < 100);
}

// ===========================================================================
//  Combined Insert + Remove tests
// ===========================================================================

TEST_CASE("InsertOptional + RemoveOptional: round-trip stability",
          "[insert_optional][remove_optional]")
{
    auto data = make_optional_instance();
    CostEvaluator eval(100);

    // Start with required clients only.
    auto sol = make_solution(data, {{0, 1}, {2, 3}});

    // Insert all profitable optional clients.
    InsertOptional ins;
    while (ins.find_best_move(sol, eval, data))
        ins.apply(sol);

    // Now try removing -- nothing should improve (insert was already optimal).
    RemoveOptional rem;
    CHECK_FALSE(rem.find_best_move(sol, eval, data));
}

TEST_CASE("InsertOptional: empty routes", "[insert_optional]")
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {.capacity = {20}});
    b.add_client({5.0, 0.0}, {.demand = {1}, .prize = 1000, .required = false});
    auto data = b.build(0);

    CostEvaluator eval(100);
    Solution sol(data);  // All routes empty, client 0 unassigned.
    CHECK(sol.num_unassigned() == 1);

    InsertOptional op;
    REQUIRE(op.find_best_move(sol, eval, data));

    op.apply(sol);
    CHECK(sol.num_unassigned() == 0);
    CHECK(sol.is_assigned(0));
}

TEST_CASE("RemoveOptional: removes last client from route",
          "[remove_optional]")
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {20}});
    // Far away, low-prize optional client.
    b.add_client({100.0, 100.0}, {.demand = {1}, .prize = 1, .required = false});
    auto data = b.build(0);

    CostEvaluator eval(100);
    auto sol = make_solution(data, {{0}});

    RemoveOptional op;
    REQUIRE(op.find_best_move(sol, eval, data));
    CHECK(op.best_delta() < 0);

    op.apply(sol);
    CHECK(sol.num_unassigned() == 1);
    CHECK(sol.route(0).empty());
}
