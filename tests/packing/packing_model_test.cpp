#include "model/packing_model.h"

#include "packing/packing_data.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

using namespace coso;

// ---------------------------------------------------------------------------
//  Adding bin types and items
// ---------------------------------------------------------------------------

TEST_CASE("PackingModel: add bin types", "[packing][model]") {
    PackingModel model;

    int b0 = model.add_bin_type({.capacity = {100}});
    REQUIRE(b0 == 0);
    REQUIRE(model.num_bin_types() == 1);

    int b1 = model.add_bin_type({.capacity = {200}, .cost = 3, .count = 5});
    REQUIRE(b1 == 1);
    REQUIRE(model.num_bin_types() == 2);

    // Check stored parameters.
    REQUIRE(model.bin_type(0).capacity == std::vector<int>{100});
    REQUIRE(model.bin_type(0).cost == 1);
    REQUIRE(model.bin_type(0).count == 0);

    REQUIRE(model.bin_type(1).capacity == std::vector<int>{200});
    REQUIRE(model.bin_type(1).cost == 3);
    REQUIRE(model.bin_type(1).count == 5);
}

TEST_CASE("PackingModel: add items", "[packing][model]") {
    PackingModel model;
    model.add_bin_type({.capacity = {100}});

    int i0 = model.add_item({.size = {30}});
    int i1 = model.add_item({.size = {50}});
    int i2 = model.add_item({.size = {20}});

    REQUIRE(i0 == 0);
    REQUIRE(i1 == 1);
    REQUIRE(i2 == 2);
    REQUIRE(model.num_items() == 3);

    REQUIRE(model.item(0).size == std::vector<int>{30});
    REQUIRE(model.item(1).size == std::vector<int>{50});
    REQUIRE(model.item(2).size == std::vector<int>{20});
}

// ---------------------------------------------------------------------------
//  Multi-dimensional capacity
// ---------------------------------------------------------------------------

TEST_CASE("PackingModel: multi-dimensional capacity", "[packing][model]") {
    PackingModel model;

    // 2D: weight and volume.
    model.add_bin_type({.capacity = {100, 200}});
    REQUIRE(model.num_dimensions() == 2);

    model.add_item({.size = {30, 50}});
    model.add_item({.size = {40, 60}});

    REQUIRE(model.num_items() == 2);
    REQUIRE(model.item(0).size == std::vector<int>{30, 50});
    REQUIRE(model.item(1).size == std::vector<int>{40, 60});
}

TEST_CASE("PackingModel: dimension mismatch throws", "[packing][model]") {
    PackingModel model;
    model.add_bin_type({.capacity = {100, 200}});  // 2D

    // Adding a 1D item should throw.
    REQUIRE_THROWS_AS(model.add_item({.size = {30}}), std::invalid_argument);

    // Adding a 3D item should also throw.
    REQUIRE_THROWS_AS(model.add_item({.size = {10, 20, 30}}), std::invalid_argument);

    // Adding a 2D bin type with different dim count should throw.
    PackingModel model2;
    model2.add_bin_type({.capacity = {100}});  // 1D
    REQUIRE_THROWS_AS(model2.add_bin_type({.capacity = {100, 200}}), std::invalid_argument);
}

TEST_CASE("PackingModel: empty capacity/size throws", "[packing][model]") {
    PackingModel model;
    REQUIRE_THROWS_AS(model.add_bin_type({.capacity = {}}), std::invalid_argument);
    REQUIRE_THROWS_AS(model.add_item({.size = {}}), std::invalid_argument);
}

// ---------------------------------------------------------------------------
//  Conflict constraints
// ---------------------------------------------------------------------------

TEST_CASE("PackingModel: add conflicts", "[packing][model]") {
    PackingModel model;
    model.add_bin_type({.capacity = {100}});
    model.add_item({.size = {30}});
    model.add_item({.size = {40}});
    model.add_item({.size = {50}});

    model.add_conflict(0, 1);
    model.add_conflict(1, 2);

    REQUIRE(model.conflicts().size() == 2);
    REQUIRE(model.conflicts()[0] == std::pair<int, int>{0, 1});
    REQUIRE(model.conflicts()[1] == std::pair<int, int>{1, 2});
}

TEST_CASE("PackingModel: conflict validation", "[packing][model]") {
    PackingModel model;
    model.add_bin_type({.capacity = {100}});
    model.add_item({.size = {30}});
    model.add_item({.size = {40}});

    // Out of range.
    REQUIRE_THROWS_AS(model.add_conflict(0, 5), std::out_of_range);
    REQUIRE_THROWS_AS(model.add_conflict(-1, 0), std::out_of_range);

    // Self-conflict.
    REQUIRE_THROWS_AS(model.add_conflict(0, 0), std::invalid_argument);
}

// ---------------------------------------------------------------------------
//  Solve returns a result
// ---------------------------------------------------------------------------

TEST_CASE("PackingModel: solve returns baseline packed result", "[packing][model]") {
    PackingModel model;
    model.add_bin_type({.capacity = {100}});
    model.add_item({.size = {30}});
    model.add_item({.size = {50}});

    Result result = model.solve(TimeLimit(1.0));

    REQUIRE(result.feasible());
    REQUIRE(result.num_bins() >= 1);
    REQUIRE(result.unassigned().empty());
    REQUIRE(result.elapsed_seconds() >= 0.0);
}

TEST_CASE("PackingModel: solve with empty model returns default", "[packing][model]") {
    PackingModel model;
    Result result = model.solve(TimeLimit(1.0));
    REQUIRE_FALSE(result.feasible());
}

// ---------------------------------------------------------------------------
//  PackingData compiled instance
// ---------------------------------------------------------------------------

TEST_CASE("PackingData: build from model", "[packing][data]") {
    PackingModel model;
    model.add_bin_type({.capacity = {100}, .cost = 2, .count = 10});
    model.add_item({.size = {30}});
    model.add_item({.size = {50}});
    model.add_item({.size = {20}});

    auto data = PackingData::build(model);

    REQUIRE(data.num_bin_types() == 1);
    REQUIRE(data.num_items() == 3);
    REQUIRE(data.num_dims() == 1);

    REQUIRE(data.bin_capacity(0, 0) == 100);
    REQUIRE(data.bin_cost(0) == 2);
    REQUIRE(data.bin_count(0) == 10);

    REQUIRE(data.item_size(0, 0) == 30);
    REQUIRE(data.item_size(1, 0) == 50);
    REQUIRE(data.item_size(2, 0) == 20);
}

TEST_CASE("PackingData: multi-dimensional", "[packing][data]") {
    PackingModel model;
    model.add_bin_type({.capacity = {100, 200}});
    model.add_item({.size = {30, 50}});
    model.add_item({.size = {40, 60}});

    auto data = PackingData::build(model);

    REQUIRE(data.num_dims() == 2);
    REQUIRE(data.bin_capacity(0, 0) == 100);
    REQUIRE(data.bin_capacity(0, 1) == 200);
    REQUIRE(data.item_size(0, 0) == 30);
    REQUIRE(data.item_size(0, 1) == 50);
    REQUIRE(data.item_size(1, 0) == 40);
    REQUIRE(data.item_size(1, 1) == 60);
}

TEST_CASE("PackingData: conflict graph", "[packing][data]") {
    PackingModel model;
    model.add_bin_type({.capacity = {100}});
    model.add_item({.size = {30}});
    model.add_item({.size = {40}});
    model.add_item({.size = {50}});

    model.add_conflict(0, 1);
    model.add_conflict(1, 2);

    auto data = PackingData::build(model);

    // Item 0 conflicts with item 1.
    REQUIRE(data.has_conflict(0, 1));
    REQUIRE(data.has_conflict(1, 0));

    // Item 1 conflicts with item 2.
    REQUIRE(data.has_conflict(1, 2));
    REQUIRE(data.has_conflict(2, 1));

    // Items 0 and 2 do not conflict.
    REQUIRE_FALSE(data.has_conflict(0, 2));

    // Adjacency list sizes.
    REQUIRE(data.conflicts(0).size() == 1);
    REQUIRE(data.conflicts(1).size() == 2);
    REQUIRE(data.conflicts(2).size() == 1);
}

// ---------------------------------------------------------------------------
//  Lower bounds
// ---------------------------------------------------------------------------

TEST_CASE("PackingData: continuous lower bound", "[packing][data]") {
    PackingModel model;
    model.add_bin_type({.capacity = {10}});

    // 7 items of size 3 => total 21, capacity 10 => ceil(21/10) = 3.
    for (int i = 0; i < 7; ++i) {
        model.add_item({.size = {3}});
    }

    auto data = PackingData::build(model);
    REQUIRE(data.continuous_lower_bound() == 3);
}

TEST_CASE("PackingData: L2 lower bound", "[packing][data]") {
    PackingModel model;
    model.add_bin_type({.capacity = {10}});

    // Items: 7, 7, 6, 6, 5, 5 (total = 36, cap = 10)
    // Continuous LB: ceil(36/10) = 4
    // L2: items > 5 (half cap): 7,7,6,6 => 4 large
    //     remaining: (10-7)+(10-7)+(10-6)+(10-6) = 3+3+4+4 = 14
    //     small: 5+5 = 10, fits in 14, no extra bins
    //     L2 = max(4, 4+0) = 4
    model.add_item({.size = {7}});
    model.add_item({.size = {7}});
    model.add_item({.size = {6}});
    model.add_item({.size = {6}});
    model.add_item({.size = {5}});
    model.add_item({.size = {5}});

    auto data = PackingData::build(model);
    REQUIRE(data.continuous_lower_bound() == 4);
    REQUIRE(data.l2_lower_bound() == 4);
}

TEST_CASE("PackingData: L2 bound tighter than continuous", "[packing][data]") {
    PackingModel model;
    model.add_bin_type({.capacity = {100}});

    // 3 items of size 51 => total = 153, cap = 100
    // Continuous: ceil(153/100) = 2
    // L2: all 3 > 50 (half cap), so large_count = 3
    //     remaining = 3*(100-51) = 147, small = 0
    //     L2 = max(2, 3) = 3
    model.add_item({.size = {51}});
    model.add_item({.size = {51}});
    model.add_item({.size = {51}});

    auto data = PackingData::build(model);
    REQUIRE(data.continuous_lower_bound() == 2);
    REQUIRE(data.l2_lower_bound() == 3);
}

// ---------------------------------------------------------------------------
//  Simple 1D bin packing instance
// ---------------------------------------------------------------------------

TEST_CASE("PackingModel: simple 1D instance end-to-end", "[packing][model]") {
    // Classic bin packing: bin capacity 10, items of various sizes.
    PackingModel model;
    model.add_bin_type({.capacity = {10}});

    model.add_item({.size = {6}});
    model.add_item({.size = {6}});
    model.add_item({.size = {5}});
    model.add_item({.size = {5}});
    model.add_item({.size = {4}});
    model.add_item({.size = {3}});
    model.add_item({.size = {3}});

    REQUIRE(model.num_bin_types() == 1);
    REQUIRE(model.num_items() == 7);
    REQUIRE(model.num_dimensions() == 1);

    // Build compiled data.
    auto data = PackingData::build(model);
    REQUIRE(data.num_items() == 7);

    // Total size = 6+6+5+5+4+3+3 = 32, cap = 10
    // Continuous LB = ceil(32/10) = 4
    REQUIRE(data.continuous_lower_bound() == 4);

    // L2: items > 5: {6, 6} => 2 large
    //     remaining: 2*(10-6) = 8
    //     small: 5+5+4+3+3 = 20, unfilled = 20-8 = 12
    //     extra = ceil(12/10) = 2
    //     L2 = max(4, 2+2) = 4
    REQUIRE(data.l2_lower_bound() == 4);

    // Solve with baseline constructive/local-search path.
    Result result = model.solve(TimeLimit(1.0));
    REQUIRE(result.feasible());
    REQUIRE(result.num_bins() >= data.continuous_lower_bound());
}

// ---------------------------------------------------------------------------
//  Variant coverage through the model API (docs/models.md §Packing)
//
//  Each case pairs the declaration with a control that drops the feature, so
//  the assertions fail if the engine ignores what was declared.
// ---------------------------------------------------------------------------

namespace {

/// Index of the returned bin holding `item`, or -1.
int bin_of(Result const& r, int item) {
    for (int b = 0; b < r.num_bins(); ++b) {
        auto const& items = r.bins()[b];
        if (std::find(items.begin(), items.end(), item) != items.end()) {
            return b;
        }
    }
    return -1;
}

}  // namespace

TEST_CASE("PackingModel: vector bin packing respects every dimension", "[packing][model]") {
    // Items 0 and 1 fit together in dimension 0 (5 + 5 <= 10) but not in
    // dimension 1 (8 + 8 > 10). Item 2 fits alongside either of them.
    auto items = std::vector<std::vector<int>>{{5, 8}, {5, 8}, {5, 1}};

    SECTION("control: dimension 0 alone packs items 0 and 1 together") {
        PackingModel model;
        model.add_bin_type({.capacity = {10}});
        for (auto const& s : items) {
            model.add_item({.size = {s[0]}});
        }

        Result result = model.solve(TimeLimit(1.0));

        REQUIRE(result.feasible());
        REQUIRE(bin_of(result, 0) == bin_of(result, 1));
    }

    SECTION("both dimensions declared") {
        PackingModel model;
        model.add_bin_type({.capacity = {10, 10}});
        for (auto const& s : items) {
            model.add_item({.size = s});
        }

        Result result = model.solve(TimeLimit(1.0));

        REQUIRE(result.feasible());
        REQUIRE(result.unassigned().empty());
        REQUIRE(result.num_bins() == 2);
        REQUIRE(result.cost() == 2.0);

        // The conflicting pair is separated by dimension 1.
        REQUIRE(bin_of(result, 0) >= 0);
        REQUIRE(bin_of(result, 1) >= 0);
        REQUIRE(bin_of(result, 0) != bin_of(result, 1));

        // Every returned bin is within capacity in both dimensions.
        for (auto const& bin : result.bins()) {
            std::vector<int> load(2, 0);
            for (int i : bin) {
                load[0] += items[i][0];
                load[1] += items[i][1];
            }
            REQUIRE(load[0] <= 10);
            REQUIRE(load[1] <= 10);
        }
    }
}

TEST_CASE("PackingModel: bin packing with conflicts keeps the pair apart", "[packing][model]") {
    // Two items of size 5 fit one bin of capacity 10 — unless they conflict.
    SECTION("control: no conflict declared") {
        PackingModel model;
        model.add_bin_type({.capacity = {10}});
        model.add_item({.size = {5}});
        model.add_item({.size = {5}});

        Result result = model.solve(TimeLimit(1.0));

        REQUIRE(result.feasible());
        REQUIRE(result.num_bins() == 1);
        REQUIRE(result.cost() == 1.0);
        REQUIRE(result.bins()[0].size() == 2);
    }

    SECTION("conflict declared") {
        PackingModel model;
        model.add_bin_type({.capacity = {10}});
        int a = model.add_item({.size = {5}});
        int b = model.add_item({.size = {5}});
        model.add_conflict(a, b);

        Result result = model.solve(TimeLimit(1.0));

        REQUIRE(result.feasible());
        REQUIRE(result.unassigned().empty());
        REQUIRE(result.num_bins() == 2);
        REQUIRE(result.cost() == 2.0);
        REQUIRE(result.bins()[0].size() == 1);
        REQUIRE(result.bins()[1].size() == 1);
        REQUIRE(bin_of(result, a) >= 0);
        REQUIRE(bin_of(result, a) != bin_of(result, b));
    }
}

TEST_CASE("PackingModel: variable-sized bin packing costs the mix", "[packing][model]") {
    // Two bin types: small (capacity 5, cost 1) and large (capacity 10,
    // cost 5). Items 8, 5, 5. Only a large bin holds the 8; the two 5s are
    // cheaper in small bins (1 + 1) than sharing a second large bin (5).
    SECTION("control: large bins only") {
        PackingModel model;
        model.add_bin_type({.capacity = {10}, .cost = 5});
        model.add_item({.size = {8}});
        model.add_item({.size = {5}});
        model.add_item({.size = {5}});

        Result result = model.solve(TimeLimit(1.0));

        REQUIRE(result.feasible());
        REQUIRE(result.num_bins() == 2);
        REQUIRE(result.cost() == 10.0);
    }

    SECTION("both bin types declared") {
        PackingModel model;
        model.add_bin_type({.capacity = {5}, .cost = 1});
        model.add_bin_type({.capacity = {10}, .cost = 5});
        int big = model.add_item({.size = {8}});
        model.add_item({.size = {5}});
        model.add_item({.size = {5}});

        Result result = model.solve(TimeLimit(1.0));

        REQUIRE(result.feasible());
        REQUIRE(result.unassigned().empty());
        // Three bins costing 7, not the two bins costing 10 that a
        // bin-count objective would return.
        REQUIRE(result.num_bins() == 3);
        REQUIRE(result.cost() == 7.0);

        // The size-8 item is alone: no small bin holds it, and no other item
        // fits beside it in a large bin.
        REQUIRE(bin_of(result, big) >= 0);
        REQUIRE(result.bins()[bin_of(result, big)].size() == 1);
    }
}

TEST_CASE("PackingModel: bin count limits the slots of a type", "[packing][model]") {
    // One small bin available, three items: two go unassigned.
    PackingModel model;
    model.add_bin_type({.capacity = {5}, .cost = 1, .count = 1});
    model.add_item({.size = {5}});
    model.add_item({.size = {5}});
    model.add_item({.size = {5}});

    Result result = model.solve(TimeLimit(1.0));

    REQUIRE(result.num_bins() == 1);
    REQUIRE(result.unassigned().size() == 2);
    REQUIRE_FALSE(result.feasible());  // unpacked items make the result infeasible
}
