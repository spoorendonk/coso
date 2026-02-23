#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "search/operator_selector.h"

#include <set>
#include <vector>

using namespace coso;
using Catch::Matchers::WithinAbs;

TEST_CASE("OperatorSelector — construction", "[operator_selector]")
{
    OperatorSelector sel(4);

    CHECK(sel.num_operators() == 4);
    CHECK(sel.total_selections() == 0);

    for (int i = 0; i < 4; ++i) {
        CHECK(sel.selections(i) == 0);
        CHECK(sel.total_reward(i) == 0.0);
        CHECK(sel.successes(i) == 0);
        CHECK(sel.avg_reward(i) == 0.0);
    }
}

TEST_CASE("OperatorSelector — round-robin for untried operators",
          "[operator_selector]")
{
    OperatorSelector sel(3);

    // First 3 selections should be 0, 1, 2 (each operator tried once).
    std::set<int> selected;
    for (int i = 0; i < 3; ++i) {
        int op = sel.select();
        CHECK(op >= 0);
        CHECK(op < 3);
        selected.insert(op);
        sel.update(op, 0.0);
    }
    CHECK(selected.size() == 3);
}

TEST_CASE("OperatorSelector — prefers high-reward operator",
          "[operator_selector]")
{
    // With exploration = 0, selector is purely greedy.
    OperatorSelector sel(3, 0.0);

    // Initialize: try each once.
    sel.update(0, 1.0);
    sel.update(1, 10.0);
    sel.update(2, 5.0);

    // With no exploration, should always pick operator 1 (highest avg reward).
    for (int i = 0; i < 10; ++i) {
        CHECK(sel.select() == 1);
        sel.update(1, 10.0);
    }
}

TEST_CASE("OperatorSelector — exploration boosts under-tried operators",
          "[operator_selector]")
{
    // High exploration parameter to make it explore.
    OperatorSelector sel(3, 100.0);

    // Give operator 0 many tries, operator 2 very few.
    for (int i = 0; i < 100; ++i)
        sel.update(0, 1.0);
    sel.update(1, 0.5);
    sel.update(2, 0.5);

    // With high exploration, under-tried operators (1, 2) should be preferred
    // over operator 0 which has been tried 100 times.
    int op = sel.select();
    CHECK(op != 0);
}

TEST_CASE("OperatorSelector — update tracks statistics", "[operator_selector]")
{
    OperatorSelector sel(2);

    sel.update(0, 5.0);
    sel.update(0, 3.0);
    sel.update(0, 0.0);
    sel.update(1, 2.0);

    CHECK(sel.total_selections() == 4);

    CHECK(sel.selections(0) == 3);
    CHECK_THAT(sel.total_reward(0), WithinAbs(8.0, 1e-12));
    CHECK(sel.successes(0) == 2);  // 5.0 and 3.0 are positive
    CHECK_THAT(sel.avg_reward(0), WithinAbs(8.0 / 3.0, 1e-12));

    CHECK(sel.selections(1) == 1);
    CHECK_THAT(sel.total_reward(1), WithinAbs(2.0, 1e-12));
    CHECK(sel.successes(1) == 1);
    CHECK_THAT(sel.avg_reward(1), WithinAbs(2.0, 1e-12));
}

TEST_CASE("OperatorSelector — reset clears all statistics",
          "[operator_selector]")
{
    OperatorSelector sel(3);

    for (int i = 0; i < 3; ++i)
        sel.update(i, static_cast<double>(i + 1));

    CHECK(sel.total_selections() == 3);

    sel.reset();

    CHECK(sel.total_selections() == 0);
    for (int i = 0; i < 3; ++i) {
        CHECK(sel.selections(i) == 0);
        CHECK(sel.total_reward(i) == 0.0);
        CHECK(sel.successes(i) == 0);
    }
}

TEST_CASE("OperatorSelector — single operator always selected",
          "[operator_selector]")
{
    OperatorSelector sel(1);

    CHECK(sel.select() == 0);
    sel.update(0, 1.0);
    CHECK(sel.select() == 0);
    sel.update(0, 0.0);
    CHECK(sel.select() == 0);
}

TEST_CASE("OperatorSelector — UCB1 converges to best operator",
          "[operator_selector]")
{
    // Moderate exploration, operator 0 is clearly the best.
    OperatorSelector sel(4, 1.0);

    // Simulate 1000 rounds: operator 0 always gives reward 10,
    // others give reward 1.
    for (int round = 0; round < 1000; ++round) {
        int op = sel.select();
        double reward = (op == 0) ? 10.0 : 1.0;
        sel.update(op, reward);
    }

    // Operator 0 should have been selected the most.
    int max_sel = 0;
    int best_op = -1;
    for (int i = 0; i < 4; ++i) {
        if (sel.selections(i) > max_sel) {
            max_sel = sel.selections(i);
            best_op = i;
        }
    }
    CHECK(best_op == 0);
    // Operator 0 should have a large majority of selections.
    CHECK(sel.selections(0) > 800);
}

TEST_CASE("OperatorSelector — custom exploration parameter",
          "[operator_selector]")
{
    // Very low exploration: should be nearly greedy after initialization.
    OperatorSelector sel(2, 0.01);

    sel.update(0, 1.0);
    sel.update(1, 100.0);

    // Next 50 selections should heavily favor operator 1.
    int count_1 = 0;
    for (int i = 0; i < 50; ++i) {
        int op = sel.select();
        if (op == 1) count_1++;
        sel.update(op, op == 1 ? 100.0 : 1.0);
    }
    CHECK(count_1 > 45);
}
