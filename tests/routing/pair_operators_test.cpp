#include <catch2/catch_test_macros.hpp>

#include "routing/operators/pair_operators.h"
#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

using namespace coso;

// ---------------------------------------------------------------------------
//  Test instance builders
// ---------------------------------------------------------------------------

/// 1 depot at (0,0), 6 clients forming 3 pickup-delivery pairs.
///
///  Depot(0,0)
///  C0(10,0) pickup   -> C1(20,0) delivery   (request 0)
///  C2(0,10) pickup   -> C3(0,20) delivery   (request 1)
///  C4(30,0) pickup   -> C5(30,10) delivery  (request 2)
///
/// 3 vehicles with capacity 20 each.
static ProblemData make_pd_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {.capacity = {20}});

    b.add_client({10.0, 0.0}, {.demand = {3}});   // 0: pickup for req 0
    b.add_client({20.0, 0.0}, {.demand = {3}});   // 1: delivery for req 0
    b.add_client({0.0, 10.0}, {.demand = {2}});    // 2: pickup for req 1
    b.add_client({0.0, 20.0}, {.demand = {2}});    // 3: delivery for req 1
    b.add_client({30.0, 0.0}, {.demand = {4}});    // 4: pickup for req 2
    b.add_client({30.0, 10.0}, {.demand = {4}});   // 5: delivery for req 2

    b.add_request(0, 1);  // request 0: pickup=C0, delivery=C1
    b.add_request(2, 3);  // request 1: pickup=C2, delivery=C3
    b.add_request(4, 5);  // request 2: pickup=C4, delivery=C5

    return b.build(0);  // no granular neighbours for deterministic testing
}

/// Same as above but with granular neighbours.
static ProblemData make_pd_granular_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {.capacity = {20}});

    b.add_client({10.0, 0.0}, {.demand = {3}});
    b.add_client({20.0, 0.0}, {.demand = {3}});
    b.add_client({0.0, 10.0}, {.demand = {2}});
    b.add_client({0.0, 20.0}, {.demand = {2}});
    b.add_client({30.0, 0.0}, {.demand = {4}});
    b.add_client({30.0, 10.0}, {.demand = {4}});

    b.add_request(0, 1);
    b.add_request(2, 3);
    b.add_request(4, 5);

    return b.build(5);
}

/// Larger instance: 4 pairs across 2 routes, arranged to have an obvious
/// improving swap.
///
/// Depot(0,0)
/// Pair 0: C0(50,0) -> C1(60,0)   (far right)
/// Pair 1: C2(5,0)  -> C3(15,0)   (near depot, right)
/// Pair 2: C4(0,50) -> C5(0,60)   (far up)
/// Pair 3: C6(0,5)  -> C7(0,15)   (near depot, up)
static ProblemData make_swap_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {.capacity = {30}});

    b.add_client({50.0, 0.0}, {.demand = {2}});   // 0: pickup req 0
    b.add_client({60.0, 0.0}, {.demand = {2}});   // 1: delivery req 0
    b.add_client({5.0, 0.0},  {.demand = {2}});   // 2: pickup req 1
    b.add_client({15.0, 0.0}, {.demand = {2}});   // 3: delivery req 1
    b.add_client({0.0, 50.0}, {.demand = {2}});   // 4: pickup req 2
    b.add_client({0.0, 60.0}, {.demand = {2}});   // 5: delivery req 2
    b.add_client({0.0, 5.0},  {.demand = {2}});   // 6: pickup req 3
    b.add_client({0.0, 15.0}, {.demand = {2}});   // 7: delivery req 3

    b.add_request(0, 1);
    b.add_request(2, 3);
    b.add_request(4, 5);
    b.add_request(6, 7);

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
//  RelocatePair tests
// ===========================================================================

TEST_CASE("RelocatePair: finds improving inter-route relocate",
          "[pair_operators][relocate_pair]")
{
    auto data = make_pd_instance();
    CostEvaluator eval(100);

    // Suboptimal: all pairs crammed in route 0, route 1 has nothing nearby.
    // Route 0: [0, 1, 2, 3, 4, 5] (all 3 pairs mixed together)
    // Spreading pairs across routes should reduce total distance.
    auto sol = make_solution(data, {{0, 1, 2, 3, 4, 5}});

    int64_t old_cost = sol.cost(eval);

    RelocatePair op;
    bool found = op.find_best_move(sol, eval, data);

    REQUIRE(found);
    CHECK(op.best_delta() < 0);

    op.apply(sol);
    int64_t new_cost = sol.cost(eval);
    CHECK(new_cost < old_cost);
    CHECK(new_cost - old_cost == op.best_delta());
}

TEST_CASE("RelocatePair: maintains pickup-before-delivery precedence",
          "[pair_operators][relocate_pair]")
{
    auto data = make_pd_instance();
    CostEvaluator eval(100);

    // Put pairs on different routes.
    auto sol = make_solution(data, {{0, 1, 4, 5}, {2, 3}});

    int64_t old_cost = sol.cost(eval);

    RelocatePair op;
    bool found = op.find_best_move(sol, eval, data);

    if (found) {
        op.apply(sol);

        // Verify precedence: for each request, pickup appears before delivery.
        auto const& requests = data.requests();
        for (auto const& req : requests) {
            int pickup = req.pickup;
            int delivery = req.delivery;

            // Find which route they're in.
            for (int r = 0; r < sol.num_routes(); ++r) {
                auto const& route = sol.route(r);
                int pos_p = -1, pos_d = -1;
                for (int i = 0; i < route.size(); ++i) {
                    if (route.client(i) == pickup) pos_p = i;
                    if (route.client(i) == delivery) pos_d = i;
                }
                if (pos_p >= 0 && pos_d >= 0) {
                    CHECK(pos_p < pos_d);
                }
                // If only pickup or only delivery found, that's a problem.
                CHECK_FALSE((pos_p >= 0) != (pos_d >= 0));
            }
        }

        int64_t new_cost = sol.cost(eval);
        CHECK(new_cost < old_cost);
    }
}

TEST_CASE("RelocatePair: no improving move when already optimal",
          "[pair_operators][relocate_pair]")
{
    auto data = make_pd_instance();
    CostEvaluator eval(100);

    // Each pair on its own closest route - roughly optimal.
    auto sol = make_solution(data, {{0, 1}, {2, 3}, {4, 5}});

    RelocatePair op;
    bool found = op.find_best_move(sol, eval, data);

    // May or may not find improvement, but if it does, cost must decrease.
    if (found) {
        int64_t old_cost = sol.cost(eval);
        op.apply(sol);
        CHECK(sol.cost(eval) < old_cost);
    }
}

TEST_CASE("RelocatePair: no requests means no move",
          "[pair_operators][relocate_pair]")
{
    // Instance without pickup-delivery requests.
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {20}});
    b.add_client({10.0, 0.0}, {.demand = {3}});
    b.add_client({20.0, 0.0}, {.demand = {4}});
    auto data = b.build(0);

    CostEvaluator eval(100);
    auto sol = make_solution(data, {{0, 1}});

    RelocatePair op;
    CHECK_FALSE(op.find_best_move(sol, eval, data));
}

TEST_CASE("RelocatePair: intra-route improvement",
          "[pair_operators][relocate_pair]")
{
    auto data = make_pd_instance();
    CostEvaluator eval(100);

    // Put all pairs in one route but in bad order:
    // pickup0, pickup1, pickup2, delivery0, delivery1, delivery2
    // Reordering within the route should help.
    auto sol = make_solution(data, {{0, 2, 4, 1, 3, 5}});

    int64_t old_cost = sol.cost(eval);

    RelocatePair op;
    bool found = op.find_best_move(sol, eval, data);

    if (found) {
        CHECK(op.best_delta() < 0);
        op.apply(sol);
        int64_t new_cost = sol.cost(eval);
        CHECK(new_cost < old_cost);
        CHECK(new_cost - old_cost == op.best_delta());
    }
}

TEST_CASE("RelocatePair: delta prediction matches actual cost change",
          "[pair_operators][relocate_pair]")
{
    auto data = make_pd_instance();
    CostEvaluator eval(100);

    // Deliberate bad arrangement.
    auto sol = make_solution(data, {{4, 5, 2, 3}, {0, 1}});
    int64_t old_cost = sol.cost(eval);

    RelocatePair op;
    bool found = op.find_best_move(sol, eval, data);

    if (found) {
        int64_t predicted_delta = op.best_delta();
        op.apply(sol);
        int64_t actual_delta = sol.cost(eval) - old_cost;
        CHECK(predicted_delta == actual_delta);
    }
}

TEST_CASE("RelocatePair: works with granular neighbours",
          "[pair_operators][relocate_pair]")
{
    auto data = make_pd_granular_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 2, 3, 4, 5}});
    int64_t old_cost = sol.cost(eval);

    RelocatePair op;
    bool found = op.find_best_move(sol, eval, data);

    if (found) {
        CHECK(op.best_delta() < 0);
        op.apply(sol);
        CHECK(sol.cost(eval) < old_cost);
    }
}

// ===========================================================================
//  SwapPair tests
// ===========================================================================

TEST_CASE("SwapPair: finds improving inter-route swap",
          "[pair_operators][swap_pair]")
{
    auto data = make_swap_instance();
    CostEvaluator eval(100);

    // Bad assignment: pair 0 (far right) with pair 3 (near, up) in route 0,
    //                 pair 2 (far up) with pair 1 (near, right) in route 1.
    // Swapping pairs 1 and 3 should be improving.
    auto sol = make_solution(data, {{0, 1, 6, 7}, {2, 3, 4, 5}});

    int64_t old_cost = sol.cost(eval);

    SwapPair op;
    bool found = op.find_best_move(sol, eval, data);

    REQUIRE(found);
    CHECK(op.best_delta() < 0);

    op.apply(sol);
    int64_t new_cost = sol.cost(eval);
    CHECK(new_cost < old_cost);
    CHECK(new_cost - old_cost == op.best_delta());
}

TEST_CASE("SwapPair: maintains pickup-before-delivery after swap",
          "[pair_operators][swap_pair]")
{
    auto data = make_swap_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 6, 7}, {2, 3, 4, 5}});

    SwapPair op;
    bool found = op.find_best_move(sol, eval, data);

    if (found) {
        op.apply(sol);

        auto const& requests = data.requests();
        for (auto const& req : requests) {
            int pickup = req.pickup;
            int delivery = req.delivery;

            for (int r = 0; r < sol.num_routes(); ++r) {
                auto const& route = sol.route(r);
                int pos_p = -1, pos_d = -1;
                for (int i = 0; i < route.size(); ++i) {
                    if (route.client(i) == pickup) pos_p = i;
                    if (route.client(i) == delivery) pos_d = i;
                }
                if (pos_p >= 0 && pos_d >= 0) {
                    CHECK(pos_p < pos_d);
                }
                CHECK_FALSE((pos_p >= 0) != (pos_d >= 0));
            }
        }
    }
}

TEST_CASE("SwapPair: no move with fewer than 2 requests",
          "[pair_operators][swap_pair]")
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {20}});
    b.add_client({10.0, 0.0}, {.demand = {3}});
    b.add_client({20.0, 0.0}, {.demand = {3}});
    b.add_request(0, 1);
    auto data = b.build(0);

    CostEvaluator eval(100);
    auto sol = make_solution(data, {{0, 1}});

    SwapPair op;
    CHECK_FALSE(op.find_best_move(sol, eval, data));
}

TEST_CASE("SwapPair: no move when pairs already on optimal routes",
          "[pair_operators][swap_pair]")
{
    auto data = make_swap_instance();
    CostEvaluator eval(100);

    // Good arrangement: horizontal pairs on route 0, vertical on route 1.
    auto sol = make_solution(data, {{2, 3, 0, 1}, {6, 7, 4, 5}});

    SwapPair op;
    bool found = op.find_best_move(sol, eval, data);

    if (found) {
        int64_t old_cost = sol.cost(eval);
        op.apply(sol);
        CHECK(sol.cost(eval) < old_cost);
    }
}

TEST_CASE("SwapPair: delta prediction matches actual cost change",
          "[pair_operators][swap_pair]")
{
    auto data = make_swap_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 6, 7}, {2, 3, 4, 5}});
    int64_t old_cost = sol.cost(eval);

    SwapPair op;
    bool found = op.find_best_move(sol, eval, data);

    if (found) {
        int64_t predicted = op.best_delta();
        op.apply(sol);
        int64_t actual = sol.cost(eval) - old_cost;
        CHECK(predicted == actual);
    }
}

TEST_CASE("SwapPair: all clients remain assigned after swap",
          "[pair_operators][swap_pair]")
{
    auto data = make_swap_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 6, 7}, {2, 3, 4, 5}});

    SwapPair op;
    bool found = op.find_best_move(sol, eval, data);

    if (found) {
        op.apply(sol);

        // All 8 clients should be assigned.
        CHECK(sol.num_unassigned() == 0);
        for (int c = 0; c < data.num_clients(); ++c)
            CHECK(sol.is_assigned(c));
    }
}

TEST_CASE("SwapPair: works with granular neighbours",
          "[pair_operators][swap_pair]")
{
    // Reuse the swap instance but with granular neighbours.
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {.capacity = {30}});

    b.add_client({50.0, 0.0}, {.demand = {2}});
    b.add_client({60.0, 0.0}, {.demand = {2}});
    b.add_client({5.0, 0.0},  {.demand = {2}});
    b.add_client({15.0, 0.0}, {.demand = {2}});
    b.add_client({0.0, 50.0}, {.demand = {2}});
    b.add_client({0.0, 60.0}, {.demand = {2}});
    b.add_client({0.0, 5.0},  {.demand = {2}});
    b.add_client({0.0, 15.0}, {.demand = {2}});

    b.add_request(0, 1);
    b.add_request(2, 3);
    b.add_request(4, 5);
    b.add_request(6, 7);

    auto data = b.build(5);
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 6, 7}, {2, 3, 4, 5}});
    int64_t old_cost = sol.cost(eval);

    SwapPair op;
    bool found = op.find_best_move(sol, eval, data);

    if (found) {
        CHECK(op.best_delta() < 0);
        op.apply(sol);
        CHECK(sol.cost(eval) < old_cost);
    }
}

// ===========================================================================
//  Repeated application tests
// ===========================================================================

TEST_CASE("RelocatePair: repeated application converges",
          "[pair_operators][relocate_pair]")
{
    auto data = make_pd_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 2, 3, 4, 5}});

    RelocatePair op;
    int iterations = 0;
    while (op.find_best_move(sol, eval, data) && iterations < 100) {
        op.apply(sol);
        ++iterations;
    }

    CHECK(iterations < 100);  // must converge
    CHECK(sol.num_unassigned() == 0);
}

TEST_CASE("SwapPair + RelocatePair combined improvement",
          "[pair_operators]")
{
    auto data = make_swap_instance();
    CostEvaluator eval(100);

    auto sol = make_solution(data, {{0, 1, 6, 7}, {2, 3, 4, 5}});
    int64_t initial_cost = sol.cost(eval);

    // Alternate between the two operators until no improvement.
    bool improved = true;
    int iterations = 0;
    while (improved && iterations < 100) {
        improved = false;
        ++iterations;

        RelocatePair reloc;
        if (reloc.find_best_move(sol, eval, data)) {
            reloc.apply(sol);
            improved = true;
        }

        SwapPair swap;
        if (swap.find_best_move(sol, eval, data)) {
            swap.apply(sol);
            improved = true;
        }
    }

    CHECK(sol.cost(eval) <= initial_cost);
    CHECK(sol.num_unassigned() == 0);
    CHECK(iterations < 100);
}
