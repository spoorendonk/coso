#include "packing/packing_solution.h"

#include "model/packing_model.h"
#include "packing/packing_data.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a simple 1D instance (items: 3,5,7,2,4, bin cap: 10)
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
//  Construction
// ---------------------------------------------------------------------------

TEST_CASE("PackingSolution: empty solution", "[packing][solution]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    REQUIRE(sol.num_bins_used() == 0);
    REQUIRE(sol.cost() == 0);
    REQUIRE(sol.num_unassigned() == 5);
    REQUIRE_FALSE(sol.all_assigned());
    REQUIRE_FALSE(sol.feasible());  // items unassigned

    // All items unassigned.
    for (int i = 0; i < 5; ++i) {
        REQUIRE(sol.item_bin(i) == -1);
    }
}

// ---------------------------------------------------------------------------
//  Assign / unassign / move
// ---------------------------------------------------------------------------

TEST_CASE("PackingSolution: assign items to bins", "[packing][solution]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    // Assign item 0 (size 3) to bin 0.
    sol.assign(0, 0);
    REQUIRE(sol.item_bin(0) == 0);
    REQUIRE(sol.bin_load(0, 0) == 3);
    REQUIRE(sol.bin_remaining(0, 0) == 7);
    REQUIRE(sol.bin_items(0).size() == 1);
    REQUIRE(sol.num_bins_used() == 1);
    REQUIRE(sol.num_unassigned() == 4);

    // Assign item 1 (size 5) to bin 0.
    sol.assign(1, 0);
    REQUIRE(sol.bin_load(0, 0) == 8);
    REQUIRE(sol.bin_remaining(0, 0) == 2);
    REQUIRE(sol.bin_items(0).size() == 2);
    REQUIRE(sol.num_bins_used() == 1);
}

TEST_CASE("PackingSolution: unassign item", "[packing][solution]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(0, 0);
    sol.assign(1, 0);
    REQUIRE(sol.bin_load(0, 0) == 8);

    sol.unassign(0);
    REQUIRE(sol.item_bin(0) == -1);
    REQUIRE(sol.bin_load(0, 0) == 5);
    REQUIRE(sol.bin_items(0).size() == 1);
    REQUIRE(sol.num_bins_used() == 1);  // bin still has item 1
    REQUIRE(sol.num_unassigned() == 4);

    // Unassign last item from bin -> bin becomes empty.
    sol.unassign(1);
    REQUIRE(sol.bin_load(0, 0) == 0);
    REQUIRE(sol.bin_items(0).empty());
    REQUIRE(sol.num_bins_used() == 0);
    REQUIRE(sol.cost() == 0);
}

TEST_CASE("PackingSolution: move item between bins", "[packing][solution]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(0, 0);  // size 3 -> bin 0
    sol.assign(1, 0);  // size 5 -> bin 0
    sol.assign(2, 1);  // size 7 -> bin 1
    REQUIRE(sol.num_bins_used() == 2);

    // Move item 1 (size 5) from bin 0 to bin 1.
    sol.move(1, 1);
    REQUIRE(sol.item_bin(1) == 1);
    REQUIRE(sol.bin_load(0, 0) == 3);   // bin 0: only item 0
    REQUIRE(sol.bin_load(1, 0) == 12);  // bin 1: item 2 (7) + item 1 (5)
    REQUIRE(sol.num_bins_used() == 2);
}

// ---------------------------------------------------------------------------
//  Cost computation
// ---------------------------------------------------------------------------

TEST_CASE("PackingSolution: cost with default bin cost", "[packing][solution]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    // Default bin cost = 1.
    sol.assign(0, 0);
    REQUIRE(sol.cost() == 1);

    sol.assign(2, 1);
    REQUIRE(sol.cost() == 2);

    sol.assign(1, 0);
    REQUIRE(sol.cost() == 2);  // same bins used
}

TEST_CASE("PackingSolution: cost with custom bin cost", "[packing][solution]") {
    PackingModel model;
    model.add_bin_type({.capacity = {10}, .cost = 5});
    model.add_item({.size = {3}});
    model.add_item({.size = {7}});
    model.add_item({.size = {4}});

    auto data = PackingData::build(model);
    PackingSolution sol(data);

    sol.assign(0, 0);
    REQUIRE(sol.cost() == 5);

    sol.assign(1, 1);
    REQUIRE(sol.cost() == 10);

    sol.assign(2, 0);
    REQUIRE(sol.cost() == 10);  // still 2 bins
}

TEST_CASE("PackingSolution: cost with multiple bin types", "[packing][solution]") {
    PackingModel model;
    model.add_bin_type({.capacity = {10}, .cost = 3, .count = 2});
    model.add_bin_type({.capacity = {20}, .cost = 7, .count = 2});
    model.add_item({.size = {5}});
    model.add_item({.size = {15}});

    auto data = PackingData::build(model);
    PackingSolution sol(data);

    // Bins 0,1 are type 0 (cost 3); bins 2,3 are type 1 (cost 7).
    sol.assign(0, 0);  // type 0 bin
    REQUIRE(sol.cost() == 3);

    sol.assign(1, 2);  // type 1 bin
    REQUIRE(sol.cost() == 10);
}

// ---------------------------------------------------------------------------
//  Delta evaluation
// ---------------------------------------------------------------------------

TEST_CASE("PackingSolution: assign_cost_delta", "[packing][solution]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    // Assigning to an empty bin opens it -> delta = bin cost (1).
    REQUIRE(sol.assign_cost_delta(0, 0) == 1);

    sol.assign(0, 0);

    // Assigning to a non-empty bin -> delta = 0.
    REQUIRE(sol.assign_cost_delta(1, 0) == 0);

    // Assigning to a different empty bin -> delta = 1.
    REQUIRE(sol.assign_cost_delta(1, 1) == 1);
}

TEST_CASE("PackingSolution: move_cost_delta", "[packing][solution]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    sol.assign(0, 0);  // bin 0 has item 0
    sol.assign(1, 0);  // bin 0 has items 0,1
    sol.assign(2, 1);  // bin 1 has item 2

    // Move item 2 from bin 1 (becomes empty) to bin 0 (already open).
    // Delta = +0 (bin 0 already open) - 1 (bin 1 closes) = -1.
    REQUIRE(sol.move_cost_delta(2, 0) == -1);

    // Move item 0 from bin 0 (still has item 1) to bin 2 (empty).
    // Delta = +1 (bin 2 opens) - 0 (bin 0 still has item 1) = +1.
    REQUIRE(sol.move_cost_delta(0, 2) == 1);

    // Move item 0 from bin 0 (still has item 1) to bin 1 (already open).
    // Delta = 0 - 0 = 0.
    REQUIRE(sol.move_cost_delta(0, 1) == 0);
}

// ---------------------------------------------------------------------------
//  Feasibility: capacity
// ---------------------------------------------------------------------------

TEST_CASE("PackingSolution: feasible packing", "[packing][solution]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    // Items: 3, 5, 7, 2, 4 (total 21), bins of cap 10.
    // Optimal: bin 0: {7, 3} = 10, bin 1: {5, 4} = 9, bin 2: {2} = 2.
    sol.assign(2, 0);  // 7
    sol.assign(0, 0);  // 3  -> bin 0 load = 10
    sol.assign(1, 1);  // 5
    sol.assign(4, 1);  // 4  -> bin 1 load = 9
    sol.assign(3, 2);  // 2  -> bin 2 load = 2

    REQUIRE(sol.feasible());
    REQUIRE(sol.num_bins_used() == 3);
    REQUIRE(sol.cost() == 3);
    REQUIRE(sol.num_capacity_violations() == 0);
    REQUIRE(sol.num_conflict_violations() == 0);
}

TEST_CASE("PackingSolution: capacity violation detected", "[packing][solution]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    // Overload bin 0: items 2 (7) + 1 (5) = 12 > 10.
    sol.assign(2, 0);
    sol.assign(1, 0);
    REQUIRE(sol.bin_load(0, 0) == 12);
    REQUIRE(sol.num_capacity_violations() > 0);
    REQUIRE_FALSE(sol.feasible());

    // Assign remaining items feasibly.
    sol.assign(0, 1);  // 3
    sol.assign(3, 1);  // 2
    sol.assign(4, 1);  // 4 -> bin 1 load = 9
    REQUIRE(sol.num_capacity_violations() > 0);
    REQUIRE_FALSE(sol.feasible());
}

TEST_CASE("PackingSolution: capacity violation resolved by move", "[packing][solution]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    // Create overloaded bin: 7 + 5 = 12 > 10.
    sol.assign(2, 0);
    sol.assign(1, 0);
    REQUIRE(sol.num_capacity_violations() > 0);

    // Move item 1 (size 5) to bin 1.
    sol.move(1, 1);
    REQUIRE(sol.bin_load(0, 0) == 7);
    REQUIRE(sol.bin_load(1, 0) == 5);

    // Assign rest.
    sol.assign(0, 1);  // 3 -> bin 1: 8
    sol.assign(3, 0);  // 2 -> bin 0: 9
    sol.assign(4, 2);  // 4

    REQUIRE(sol.num_capacity_violations() == 0);
    REQUIRE(sol.feasible());
}

// ---------------------------------------------------------------------------
//  Feasibility: conflicts
// ---------------------------------------------------------------------------

TEST_CASE("PackingSolution: conflict violation detected", "[packing][solution]") {
    PackingModel model;
    model.add_bin_type({.capacity = {100}});
    model.add_item({.size = {10}});  // 0
    model.add_item({.size = {10}});  // 1
    model.add_item({.size = {10}});  // 2

    model.add_conflict(0, 1);  // items 0 and 1 cannot share a bin

    auto data = PackingData::build(model);
    PackingSolution sol(data);

    // Put conflicting items in the same bin.
    sol.assign(0, 0);
    sol.assign(1, 0);
    sol.assign(2, 0);

    REQUIRE(sol.num_conflict_violations() == 1);
    REQUIRE_FALSE(sol.feasible());
}

TEST_CASE("PackingSolution: conflict resolved by move", "[packing][solution]") {
    PackingModel model;
    model.add_bin_type({.capacity = {100}});
    model.add_item({.size = {10}});  // 0
    model.add_item({.size = {10}});  // 1
    model.add_item({.size = {10}});  // 2

    model.add_conflict(0, 1);

    auto data = PackingData::build(model);
    PackingSolution sol(data);

    sol.assign(0, 0);
    sol.assign(1, 0);
    REQUIRE(sol.num_conflict_violations() == 1);

    // Move item 1 to a different bin.
    sol.move(1, 1);
    REQUIRE(sol.num_conflict_violations() == 0);

    sol.assign(2, 0);
    REQUIRE(sol.feasible());
}

TEST_CASE("PackingSolution: multiple conflicts in same bin", "[packing][solution]") {
    PackingModel model;
    model.add_bin_type({.capacity = {100}});
    model.add_item({.size = {10}});  // 0
    model.add_item({.size = {10}});  // 1
    model.add_item({.size = {10}});  // 2

    model.add_conflict(0, 1);
    model.add_conflict(0, 2);
    model.add_conflict(1, 2);

    auto data = PackingData::build(model);
    PackingSolution sol(data);

    sol.assign(0, 0);
    sol.assign(1, 0);
    sol.assign(2, 0);

    // 3 conflict pairs all in same bin.
    REQUIRE(sol.num_conflict_violations() == 3);

    // Move item 2 out -> removes conflicts (0,2) and (1,2).
    sol.move(2, 1);
    REQUIRE(sol.num_conflict_violations() == 1);  // only (0,1) remains

    sol.move(1, 1);
    // Now bin 0 has {0}, bin 1 has {2,1}. Conflict (1,2) in bin 1.
    REQUIRE(sol.num_conflict_violations() == 1);

    sol.move(1, 2);
    REQUIRE(sol.num_conflict_violations() == 0);
}

// ---------------------------------------------------------------------------
//  Multi-dimensional packing
// ---------------------------------------------------------------------------

TEST_CASE("PackingSolution: 2D packing", "[packing][solution]") {
    PackingModel model;
    // 2D: weight and volume.
    model.add_bin_type({.capacity = {10, 20}});
    model.add_item({.size = {3, 8}});   // 0
    model.add_item({.size = {5, 10}});  // 1
    model.add_item({.size = {4, 7}});   // 2

    auto data = PackingData::build(model);
    PackingSolution sol(data);

    sol.assign(0, 0);
    REQUIRE(sol.bin_load(0, 0) == 3);
    REQUIRE(sol.bin_load(0, 1) == 8);
    REQUIRE(sol.bin_remaining(0, 0) == 7);
    REQUIRE(sol.bin_remaining(0, 1) == 12);

    sol.assign(1, 0);
    REQUIRE(sol.bin_load(0, 0) == 8);
    REQUIRE(sol.bin_load(0, 1) == 18);

    // Item 2 fits in weight (8+4=12 > 10), so capacity violation.
    sol.assign(2, 0);
    REQUIRE(sol.bin_load(0, 0) == 12);
    REQUIRE(sol.bin_load(0, 1) == 25);
    REQUIRE(sol.num_capacity_violations() == 2);  // both dims violated
    REQUIRE_FALSE(sol.feasible());
}

TEST_CASE("PackingSolution: 2D feasible packing", "[packing][solution]") {
    PackingModel model;
    model.add_bin_type({.capacity = {10, 20}});
    model.add_item({.size = {3, 8}});   // 0
    model.add_item({.size = {5, 10}});  // 1
    model.add_item({.size = {4, 7}});   // 2

    auto data = PackingData::build(model);
    PackingSolution sol(data);

    // Bin 0: items 0,2 -> weight 7, volume 15 (fits).
    sol.assign(0, 0);
    sol.assign(2, 0);
    REQUIRE(sol.bin_load(0, 0) == 7);
    REQUIRE(sol.bin_load(0, 1) == 15);

    // Bin 1: item 1 -> weight 5, volume 10 (fits).
    sol.assign(1, 1);
    REQUIRE(sol.feasible());
    REQUIRE(sol.num_bins_used() == 2);
}

// ---------------------------------------------------------------------------
//  item_fits queries
// ---------------------------------------------------------------------------

TEST_CASE("PackingSolution: item_fits checks", "[packing][solution]") {
    auto data = make_simple_1d();
    PackingSolution sol(data);

    // Bin 0 is empty, cap 10. Item 2 (size 7) fits.
    REQUIRE(sol.item_fits(2, 0));

    sol.assign(2, 0);  // bin 0 load = 7

    // Item 1 (size 5) does not fit (7+5=12 > 10).
    REQUIRE_FALSE(sol.item_fits_capacity(1, 0));
    REQUIRE_FALSE(sol.item_fits(1, 0));

    // Item 0 (size 3) fits (7+3=10 <= 10).
    REQUIRE(sol.item_fits_capacity(0, 0));
    REQUIRE(sol.item_fits(0, 0));
}

TEST_CASE("PackingSolution: item_fits with conflicts", "[packing][solution]") {
    PackingModel model;
    model.add_bin_type({.capacity = {100}});
    model.add_item({.size = {10}});  // 0
    model.add_item({.size = {10}});  // 1
    model.add_conflict(0, 1);

    auto data = PackingData::build(model);
    PackingSolution sol(data);

    sol.assign(0, 0);

    // Item 1 fits capacity but conflicts with item 0.
    REQUIRE(sol.item_fits_capacity(1, 0));
    REQUIRE(sol.has_conflict_in_bin(1, 0));
    REQUIRE_FALSE(sol.item_fits(1, 0));

    // Item 1 fits in a different bin.
    REQUIRE(sol.item_fits(1, 1));
}

// ---------------------------------------------------------------------------
//  Bin type mapping
// ---------------------------------------------------------------------------

TEST_CASE("PackingSolution: bin type mapping correct", "[packing][solution]") {
    PackingModel model;
    model.add_bin_type({.capacity = {10}, .cost = 2, .count = 3});
    model.add_bin_type({.capacity = {20}, .cost = 5, .count = 2});
    model.add_item({.size = {5}});

    auto data = PackingData::build(model);
    PackingSolution sol(data);

    // 5 total bins: slots 0-2 are type 0, slots 3-4 are type 1.
    REQUIRE(sol.num_bins() == 5);
    REQUIRE(sol.bin_type(0) == 0);
    REQUIRE(sol.bin_type(1) == 0);
    REQUIRE(sol.bin_type(2) == 0);
    REQUIRE(sol.bin_type(3) == 1);
    REQUIRE(sol.bin_type(4) == 1);

    sol.assign(0, 3);  // use a type-1 bin
    REQUIRE(sol.cost() == 5);
}
