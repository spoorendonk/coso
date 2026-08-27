#include "packing/packing_operators.h"

#include "model/packing_model.h"
#include "packing/packing_data.h"
#include "packing/packing_solution.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

/// 1D instance: items {3, 5, 7, 2, 4}, bin cap 10, cost 1.
static PackingData make_simple_1d() {
    PackingModel model;
    model.add_bin_type({.capacity = {10}});
    model.add_item({.size = {3}});  // 0
    model.add_item({.size = {5}});  // 1
    model.add_item({.size = {7}});  // 2
    model.add_item({.size = {2}});  // 3
    model.add_item({.size = {4}});  // 4
    return PackingData::build(model);
}

/// 1D instance with conflicts: items {10, 10, 10}, cap 100, conflict(0,1).
static PackingData make_conflict_instance() {
    PackingModel model;
    model.add_bin_type({.capacity = {100}});
    model.add_item({.size = {10}});  // 0
    model.add_item({.size = {10}});  // 1
    model.add_item({.size = {10}});  // 2
    model.add_conflict(0, 1);
    return PackingData::build(model);
}

// ---------------------------------------------------------------------------
//  MoveItem tests
// ---------------------------------------------------------------------------

TEST_CASE("MoveItem: evaluate delta", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(0, 0);  // size 3 -> bin 0
    sol.assign(1, 0);  // size 5 -> bin 0
    sol.assign(2, 1);  // size 7 -> bin 1

    // Move item 2 from bin 1 (sole item) to bin 0 (already open).
    // bin 1 closes: delta = -1.
    auto move = evaluate_move(sol, 2, 0);
    REQUIRE(move.item == 2);
    REQUIRE(move.to_bin == 0);
    REQUIRE(move.delta == -1);

    // Move item 0 from bin 0 (has 2 items) to bin 2 (empty, opens).
    // delta = +1.
    auto move2 = evaluate_move(sol, 0, 2);
    REQUIRE(move2.delta == 1);
}

TEST_CASE("MoveItem: feasibility check", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(2, 0);  // size 7 -> bin 0
    sol.assign(1, 1);  // size 5 -> bin 1

    // Move item 1 (size 5) to bin 0 (load 7): 7+5=12 > 10, infeasible.
    auto move = evaluate_move(sol, 1, 0);
    REQUIRE_FALSE(is_feasible(sol, move));

    // Move item 1 (size 5) to bin 2 (empty, cap 10): feasible.
    auto move2 = evaluate_move(sol, 1, 2);
    REQUIRE(is_feasible(sol, move2));
}

TEST_CASE("MoveItem: apply", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(0, 0);
    sol.assign(1, 0);
    sol.assign(2, 1);

    int cost_before = sol.cost();
    auto move = evaluate_move(sol, 2, 0);
    apply(sol, move);

    REQUIRE(sol.item_bin(2) == 0);
    REQUIRE(sol.bin_items(1).empty());
    REQUIRE(sol.cost() == cost_before + move.delta);
}

TEST_CASE("MoveItem: enumerate returns feasible moves", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(0, 0);  // 3
    sol.assign(3, 0);  // 2 -> bin 0 load = 5
    sol.assign(1, 1);  // 5 -> bin 1 load = 5

    auto moves = enumerate_moves(sol);
    REQUIRE(!moves.empty());

    // Every enumerated move must be feasible.
    for (auto const& m : moves) {
        REQUIRE(is_feasible(sol, m));
    }
}

TEST_CASE("MoveItem: delta accuracy", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(0, 0);  // 3
    sol.assign(1, 1);  // 5
    sol.assign(2, 2);  // 7
    sol.assign(3, 0);  // 2
    sol.assign(4, 1);  // 4

    auto moves = enumerate_moves(sol);
    for (auto const& m : moves) {
        // Verify delta by applying and checking.
        PackingSolution copy = sol;
        int cost_before = copy.cost();
        apply(copy, m);
        REQUIRE(copy.cost() == cost_before + m.delta);
    }
}

// ---------------------------------------------------------------------------
//  SwapItems tests
// ---------------------------------------------------------------------------

TEST_CASE("SwapItems: evaluate delta is zero", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(0, 0);  // 3 -> bin 0
    sol.assign(2, 1);  // 7 -> bin 1

    auto swap = evaluate_swap(sol, 0, 2);
    // Swap does not open/close bins -> delta = 0.
    REQUIRE(swap.delta == 0);
}

TEST_CASE("SwapItems: feasibility - capacity", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(0, 0);  // 3 -> bin 0
    sol.assign(3, 0);  // 2 -> bin 0 (load=5)
    sol.assign(2, 1);  // 7 -> bin 1 (load=7)

    // Swap item 3 (size 2) in bin 0 with item 2 (size 7) in bin 1.
    // bin 0: 5-2+7=10 ok. bin 1: 7-7+2=2 ok. Feasible.
    auto swap1 = evaluate_swap(sol, 3, 2);
    REQUIRE(is_feasible(sol, swap1));

    // Swap item 0 (size 3) in bin 0 with item 2 (size 7) in bin 1.
    // bin 0: 5-3+7=9 ok. bin 1: 7-7+3=3 ok. Feasible.
    auto swap2 = evaluate_swap(sol, 0, 2);
    REQUIRE(is_feasible(sol, swap2));
}

TEST_CASE("SwapItems: feasibility - capacity violation", "[packing][operators]") {
    PackingModel model;
    model.add_bin_type({.capacity = {10}});
    model.add_item({.size = {3}});  // 0
    model.add_item({.size = {4}});  // 1
    model.add_item({.size = {8}});  // 2
    model.add_item({.size = {9}});  // 3

    auto data = PackingData::build(model);
    PackingSolution sol(data);

    sol.assign(0, 0);  // 3
    sol.assign(1, 0);  // 4 -> bin 0 load=7
    sol.assign(2, 1);  // 8
    sol.assign(3, 1);  // 9 -> bin 1 load=17 (overloaded)

    // Swap item 1 (size 4) with item 3 (size 9).
    // bin 0: 7-4+9=12 > 10 => infeasible.
    auto swap = evaluate_swap(sol, 1, 3);
    REQUIRE_FALSE(is_feasible(sol, swap));
}

TEST_CASE("SwapItems: apply", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(0, 0);  // 3 -> bin 0
    sol.assign(2, 1);  // 7 -> bin 1

    auto swap = evaluate_swap(sol, 0, 2);
    apply(sol, swap);

    REQUIRE(sol.item_bin(0) == 1);  // item 0 now in bin 1
    REQUIRE(sol.item_bin(2) == 0);  // item 2 now in bin 0
    REQUIRE(sol.bin_load(0, 0) == 7);
    REQUIRE(sol.bin_load(1, 0) == 3);
}

TEST_CASE("SwapItems: enumerate returns feasible swaps", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(0, 0);  // 3
    sol.assign(3, 0);  // 2 -> bin 0 load=5
    sol.assign(1, 1);  // 5 -> bin 1 load=5

    auto swaps = enumerate_swaps(sol);

    for (auto const& s : swaps) {
        REQUIRE(is_feasible(sol, s));
    }
}

TEST_CASE("SwapItems: delta accuracy", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(0, 0);
    sol.assign(1, 1);
    sol.assign(2, 2);
    sol.assign(3, 0);
    sol.assign(4, 1);

    auto swaps = enumerate_swaps(sol);
    for (auto const& s : swaps) {
        PackingSolution copy = sol;
        int cost_before = copy.cost();
        apply(copy, s);
        REQUIRE(copy.cost() == cost_before + s.delta);
    }
}

// ---------------------------------------------------------------------------
//  MergeBins tests
// ---------------------------------------------------------------------------

TEST_CASE("MergeBins: evaluate delta", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(0, 0);  // 3 -> bin 0
    sol.assign(3, 1);  // 2 -> bin 1

    // Merge bin 1 into bin 0: bin 1 closes, delta = -1.
    auto merge = evaluate_merge(sol, 1, 0);
    REQUIRE(merge.delta == -1);
}

TEST_CASE("MergeBins: feasibility - fits", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(0, 0);  // 3 -> bin 0
    sol.assign(3, 1);  // 2 -> bin 1

    // 3+2=5 <= 10: feasible.
    auto merge = evaluate_merge(sol, 1, 0);
    REQUIRE(is_feasible(sol, merge));
}

TEST_CASE("MergeBins: feasibility - does not fit", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(2, 0);  // 7 -> bin 0
    sol.assign(1, 1);  // 5 -> bin 1

    // 7+5=12 > 10: infeasible.
    auto merge = evaluate_merge(sol, 1, 0);
    REQUIRE_FALSE(is_feasible(sol, merge));
}

TEST_CASE("MergeBins: apply", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(0, 0);  // 3
    sol.assign(3, 1);  // 2

    int cost_before = sol.cost();
    auto merge = evaluate_merge(sol, 1, 0);
    apply(sol, merge);

    REQUIRE(sol.bin_items(1).empty());
    REQUIRE(sol.bin_load(0, 0) == 5);
    REQUIRE(sol.num_bins_used() == 1);
    REQUIRE(sol.cost() == cost_before + merge.delta);
}

TEST_CASE("MergeBins: enumerate returns feasible merges", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(0, 0);  // 3
    sol.assign(3, 1);  // 2
    sol.assign(4, 2);  // 4

    auto merges = enumerate_merges(sol);
    REQUIRE(!merges.empty());

    for (auto const& m : merges) {
        REQUIRE(is_feasible(sol, m));
    }
}

TEST_CASE("MergeBins: delta accuracy", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(0, 0);
    sol.assign(3, 1);
    sol.assign(4, 2);

    auto merges = enumerate_merges(sol);
    for (auto const& m : merges) {
        PackingSolution copy = sol;
        int cost_before = copy.cost();
        apply(copy, m);
        REQUIRE(copy.cost() == cost_before + m.delta);
    }
}

// ---------------------------------------------------------------------------
//  SplitBin tests
// ---------------------------------------------------------------------------

TEST_CASE("SplitBin: evaluate delta", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    // Overload bin 0: 7+5 = 12 > 10.
    sol.assign(2, 0);  // 7
    sol.assign(1, 0);  // 5

    // Split: move item 1 to bin 1 (empty). Opens bin 1: delta = +1.
    auto split = evaluate_split(sol, 0, 1, {1});
    REQUIRE(split.delta == 1);
}

TEST_CASE("SplitBin: feasibility", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(2, 0);  // 7
    sol.assign(1, 0);  // 5

    // Move item 1 (size 5) to empty bin 1 (cap 10): fits.
    auto split = evaluate_split(sol, 0, 1, {1});
    REQUIRE(is_feasible(sol, split));
}

TEST_CASE("SplitBin: apply resolves overload", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(2, 0);  // 7
    sol.assign(1, 0);  // 5
    REQUIRE(sol.num_capacity_violations() > 0);

    auto split = evaluate_split(sol, 0, 1, {1});
    apply(sol, split);

    REQUIRE(sol.bin_load(0, 0) == 7);
    REQUIRE(sol.bin_load(1, 0) == 5);
    REQUIRE(sol.num_bins_used() == 2);
    // No capacity violations after split.
    REQUIRE(sol.num_capacity_violations() == 0);
}

TEST_CASE("SplitBin: enumerate finds splits for overloaded bins", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    // Create overloaded bin.
    sol.assign(2, 0);  // 7
    sol.assign(1, 0);  // 5
    sol.assign(0, 0);  // 3 -> bin 0 load=15

    // Assign remaining items to avoid them being unassigned.
    sol.assign(3, 1);  // 2
    sol.assign(4, 1);  // 4

    auto splits = enumerate_splits(sol);
    REQUIRE(!splits.empty());

    for (auto const& s : splits) {
        REQUIRE(is_feasible(sol, s));
        REQUIRE(s.source_bin == 0);  // only bin 0 is overloaded
    }
}

TEST_CASE("SplitBin: delta accuracy", "[packing][operators]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(2, 0);  // 7
    sol.assign(1, 0);  // 5

    auto split = evaluate_split(sol, 0, 1, {1});
    PackingSolution copy = sol;
    int cost_before = copy.cost();
    apply(copy, split);
    REQUIRE(copy.cost() == cost_before + split.delta);
}

// ---------------------------------------------------------------------------
//  Conflict-aware tests
// ---------------------------------------------------------------------------

TEST_CASE("MoveItem: conflict prevents move", "[packing][operators]") {
    auto data = make_conflict_instance();
    PackingSolution sol(data);

    sol.assign(0, 0);  // item 0 in bin 0
    sol.assign(1, 1);  // item 1 in bin 1
    sol.assign(2, 1);  // item 2 in bin 1

    // Move item 1 to bin 0: conflict with item 0.
    auto move = evaluate_move(sol, 1, 0);
    REQUIRE_FALSE(is_feasible(sol, move));

    // Move item 2 to bin 0: no conflict with item 0.
    auto move2 = evaluate_move(sol, 2, 0);
    REQUIRE(is_feasible(sol, move2));
}

TEST_CASE("SwapItems: conflict prevents swap", "[packing][operators]") {
    PackingModel model;
    model.add_bin_type({.capacity = {100}});
    model.add_item({.size = {10}});  // 0
    model.add_item({.size = {10}});  // 1
    model.add_item({.size = {10}});  // 2
    model.add_item({.size = {10}});  // 3
    model.add_conflict(0, 2);        // 0 and 2 cannot share a bin
    model.add_conflict(1, 3);        // 1 and 3 cannot share a bin

    auto data = PackingData::build(model);
    PackingSolution sol(data);

    sol.assign(0, 0);  // bin 0: {0, 1}
    sol.assign(1, 0);
    sol.assign(2, 1);  // bin 1: {2, 3}
    sol.assign(3, 1);

    // Swap item 1 (bin 0) with item 2 (bin 1):
    // bin 0 would have {0, 2} -> conflict(0,2). Infeasible.
    auto swap1 = evaluate_swap(sol, 1, 2);
    REQUIRE_FALSE(is_feasible(sol, swap1));

    // Swap item 0 (bin 0) with item 2 (bin 1):
    // bin 0 would have {1, 2} -> no conflict. bin 1 would have {0, 3} -> no conflict. Feasible.
    auto swap2 = evaluate_swap(sol, 0, 2);
    REQUIRE(is_feasible(sol, swap2));
}

TEST_CASE("MergeBins: conflict prevents merge", "[packing][operators]") {
    auto data = make_conflict_instance();
    PackingSolution sol(data);

    sol.assign(0, 0);  // bin 0: {0}
    sol.assign(1, 1);  // bin 1: {1}
    sol.assign(2, 0);  // bin 0: {0, 2}

    // Merge bin 1 into bin 0: would put {0,1} together -> conflict.
    auto merge = evaluate_merge(sol, 1, 0);
    REQUIRE_FALSE(is_feasible(sol, merge));

    // Merge bin 0 into bin 1: would put {0,1} together -> conflict.
    auto merge2 = evaluate_merge(sol, 0, 1);
    REQUIRE_FALSE(is_feasible(sol, merge2));
}

// ---------------------------------------------------------------------------
//  Multi-dimensional tests
// ---------------------------------------------------------------------------

TEST_CASE("MoveItem: 2D feasibility check", "[packing][operators]") {
    PackingModel model;
    model.add_bin_type({.capacity = {10, 20}});
    model.add_item({.size = {3, 15}});  // 0: fits weight but tight on volume
    model.add_item({.size = {2, 8}});   // 1

    auto data = PackingData::build(model);
    PackingSolution sol(data);

    sol.assign(0, 0);  // bin 0: load=(3,15)
    sol.assign(1, 1);  // bin 1: load=(2,8)

    // Move item 1 to bin 0: weight 3+2=5<=10, volume 15+8=23>20. Infeasible.
    auto move = evaluate_move(sol, 1, 0);
    REQUIRE_FALSE(is_feasible(sol, move));
}
