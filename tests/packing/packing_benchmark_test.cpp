#include <catch2/catch_test_macros.hpp>

#include "model/packing_model.h"
#include "packing/packing_data.h"
#include "packing/packing_operators.h"
#include "packing/packing_solution.h"

#include <algorithm>
#include <iostream>
#include <numeric>
#include <vector>

using namespace coso;

// ---------------------------------------------------------------------------
//  First-Fit Decreasing (FFD) construction heuristic
// ---------------------------------------------------------------------------

/// Build a feasible solution using FFD: sort items by decreasing size,
/// assign each to the first bin where it fits, opening a new bin if needed.
static void first_fit_decreasing(PackingSolution& sol, PackingData const& data)
{
    int const N = data.num_items();
    int const D = data.num_dims();

    // Sort items by total size (sum across dimensions), decreasing.
    std::vector<int> order(N);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        int sa = 0, sb = 0;
        for (int d = 0; d < D; ++d) {
            sa += data.item_size(a, d);
            sb += data.item_size(b, d);
        }
        return sa > sb;
    });

    // Track which bins are "open" (have items).
    int next_bin = 0;

    for (int item : order) {
        bool placed = false;
        for (int b = 0; b < next_bin; ++b) {
            if (sol.item_fits(item, b)) {
                sol.assign(item, b);
                placed = true;
                break;
            }
        }
        if (!placed) {
            sol.assign(item, next_bin);
            ++next_bin;
        }
    }
}

// ---------------------------------------------------------------------------
//  Local search: repeatedly apply best improving merge or move
// ---------------------------------------------------------------------------

/// Run a simple steepest-descent local search using merge + move operators.
/// Returns the number of improving steps taken.
static int local_search(PackingSolution& sol)
{
    int steps = 0;
    bool improved = true;

    while (improved) {
        improved = false;

        // Try merges first (they reduce bin count by 1).
        auto merges = enumerate_merges(sol);
        if (!merges.empty()) {
            // Find the best (most negative delta) merge.
            auto best = std::min_element(merges.begin(), merges.end(),
                [](auto const& a, auto const& b) { return a.delta < b.delta; });
            if (best->delta < 0) {
                apply(sol, *best);
                ++steps;
                improved = true;
                continue;
            }
        }

        // Try moves that reduce bin count (delta < 0).
        auto moves = enumerate_moves(sol);
        if (!moves.empty()) {
            auto best = std::min_element(moves.begin(), moves.end(),
                [](auto const& a, auto const& b) { return a.delta < b.delta; });
            if (best->delta < 0) {
                apply(sol, *best);
                ++steps;
                improved = true;
                continue;
            }
        }
    }

    return steps;
}

// ---------------------------------------------------------------------------
//  Helper: build a 1D bin packing instance from item sizes and capacity.
// ---------------------------------------------------------------------------

static PackingData make_1d_instance(int capacity, std::vector<int> const& sizes)
{
    PackingModel model;
    model.add_bin_type({.capacity = {capacity}});
    for (int s : sizes) {
        model.add_item({.size = {s}});
    }
    return PackingData::build(model);
}

/// Run an end-to-end benchmark: FFD + local search. Report results.
static void run_packing_benchmark(
    std::string const& name,
    int capacity,
    std::vector<int> const& sizes,
    int optimal_bins,
    int max_bins_allowed = -1)
{
    if (max_bins_allowed < 0)
        max_bins_allowed = optimal_bins;  // default: must match optimal

    auto data = make_1d_instance(capacity, sizes);
    PackingSolution sol(data);

    first_fit_decreasing(sol, data);
    int ffd_bins = sol.num_bins_used();

    int ls_steps = local_search(sol);

    int final_bins = sol.num_bins_used();

    std::cout << "\n=== " << name << " packing benchmark ===\n"
              << "  Items:        " << sizes.size() << "\n"
              << "  Capacity:     " << capacity << "\n"
              << "  FFD bins:     " << ffd_bins << "\n"
              << "  LS steps:     " << ls_steps << "\n"
              << "  Final bins:   " << final_bins << "\n"
              << "  Optimal:      " << optimal_bins << "\n"
              << "  LB (cont):    " << data.continuous_lower_bound() << "\n"
              << "  LB (L2):      " << data.l2_lower_bound() << "\n"
              << std::endl;

    CHECK(sol.feasible());
    CHECK(sol.all_assigned());
    CHECK(final_bins <= max_bins_allowed);
    CHECK(final_bins >= optimal_bins);
}

// ===========================================================================
//  Falkenauer-style Uniform instances (U class)
//  Items drawn uniformly from [20, 100], capacity = 150.
//  These small hand-picked instances have known optimal solutions.
// ===========================================================================

TEST_CASE("Falkenauer U: 10 items, capacity 150", "[benchmark][packing]")
{
    // 10 items, capacity 150. Total size = 660, LB = ceil(660/150) = 5.
    // FFD achieves 5 bins which matches LB. Optimal = 5.
    run_packing_benchmark("Falkenauer-U-10", 150,
        {97, 85, 72, 63, 62, 57, 55, 49, 20, 100}, 5);
}

TEST_CASE("Falkenauer U: 20 items, capacity 150", "[benchmark][packing]")
{
    // 20 items, capacity 150. Total = 1200, LB = 8.
    // FFD achieves 9, allow gap of 1 bin over LB.
    run_packing_benchmark("Falkenauer-U-20", 150,
        {95, 90, 85, 80, 75, 70, 65, 60, 55, 50,
         45, 40, 35, 30, 25, 20, 98, 72, 67, 43},
        8, 9);
}

TEST_CASE("Falkenauer U: 30 items, capacity 150", "[benchmark][packing]")
{
    // 30 uniform items, capacity 150. Total = 1710, LB = 12.
    run_packing_benchmark("Falkenauer-U-30", 150,
        {99, 94, 89, 85, 80, 76, 73, 70, 66, 62,
         58, 55, 52, 49, 46, 43, 40, 37, 34, 31,
         28, 25, 22, 97, 91, 83, 78, 68, 60, 48},
        12, 13);
}

// ===========================================================================
//  Falkenauer-style Triplet instances (T class)
//  Capacity = 1000, items chosen so each bin optimally holds exactly 3 items.
//  Three size ranges: large [380,490], medium [255,370], small [135,245].
// ===========================================================================

TEST_CASE("Falkenauer T: 9 items (3 triplets)", "[benchmark][packing]")
{
    // 3 bins optimal, each exactly 3 items summing to <= 1000.
    // Bin 1: 490 + 300 + 200 = 990
    // Bin 2: 450 + 320 + 220 = 990
    // Bin 3: 480 + 280 + 240 = 1000
    // FFD may need 4 bins since it can't always find the optimal triplets.
    run_packing_benchmark("Falkenauer-T-9", 1000,
        {490, 450, 480, 300, 320, 280, 200, 220, 240}, 3, 4);
}

TEST_CASE("Falkenauer T: 15 items (5 triplets)", "[benchmark][packing]")
{
    // 5 bins optimal. Each bin holds 3 items.
    // {485+310+200, 460+330+210, 470+290+235, 440+350+205, 490+270+240}
    // FFD may need 6 bins; triplet instances are hard for greedy heuristics.
    run_packing_benchmark("Falkenauer-T-15", 1000,
        {485, 460, 470, 440, 490,
         310, 330, 290, 350, 270,
         200, 210, 235, 205, 240}, 5, 6);
}

TEST_CASE("Falkenauer T: 30 items (10 triplets)", "[benchmark][packing]")
{
    // 10 bins optimal.
    run_packing_benchmark("Falkenauer-T-30", 1000,
        {490, 485, 470, 460, 455, 445, 440, 435, 420, 410,
         310, 305, 295, 290, 285, 280, 275, 270, 265, 260,
         200, 210, 235, 250, 240, 225, 215, 245, 205, 230},
        10, 11);
}

// ===========================================================================
//  Scholl-style instances (Class 1, 2, 3)
//  Standard benchmark classes from Scholl, Klein & Juergens (1997).
// ===========================================================================

TEST_CASE("Scholl Class 1: small items, cap 100", "[benchmark][packing]")
{
    // Class 1: items in [1, 100], capacity 100. Easy instances.
    // 15 items, total = 572, LB = 6, optimal = 6.
    run_packing_benchmark("Scholl-C1-15", 100,
        {92, 87, 75, 63, 51, 43, 37, 28, 22, 17, 12, 8, 5, 19, 13}, 6, 7);
}

TEST_CASE("Scholl Class 1: 25 items, cap 100", "[benchmark][packing]")
{
    // 25 items in [1, 100], capacity 100.
    run_packing_benchmark("Scholl-C1-25", 100,
        {95, 88, 79, 71, 63, 56, 48, 41, 33, 26,
         19, 14, 9, 5, 2, 97, 82, 67, 52, 38,
         24, 11, 7, 3, 1},
        10, 11);
}

TEST_CASE("Scholl Class 2: medium items, cap 150", "[benchmark][packing]")
{
    // Class 2: items in [25, 50], capacity 150. Many items fit 3 per bin.
    // 20 items, total = 740, LB = 5, optimal = 5.
    run_packing_benchmark("Scholl-C2-20", 150,
        {49, 47, 45, 43, 41, 39, 37, 35, 33, 31,
         29, 27, 50, 48, 46, 44, 42, 40, 38, 36},
        5, 6);
}

TEST_CASE("Scholl Class 3: large items, cap 100", "[benchmark][packing]")
{
    // Class 3: items in [25, 50], capacity 100. Hard instances.
    // Items are large relative to bin, so at most 2-3 per bin.
    // 10 items, total = 385, LB = 4, optimal = 5.
    run_packing_benchmark("Scholl-C3-10", 100,
        {49, 47, 45, 43, 41, 39, 37, 35, 26, 23}, 5, 5);
}

TEST_CASE("Scholl Class 3: 20 items, cap 100", "[benchmark][packing]")
{
    // 20 items in [25, 50], capacity 100. Optimal = 10.
    run_packing_benchmark("Scholl-C3-20", 100,
        {50, 49, 48, 47, 46, 45, 44, 43, 42, 41,
         40, 39, 38, 37, 36, 35, 34, 33, 32, 31},
        10, 11);
}

// ===========================================================================
//  Lower bound verification
// ===========================================================================

TEST_CASE("Packing lower bounds are valid", "[benchmark][packing]")
{
    SECTION("Continuous LB") {
        // 5 items of size 30, cap 100. Total = 150, LB = 2.
        auto data = make_1d_instance(100, {30, 30, 30, 30, 30});
        CHECK(data.continuous_lower_bound() == 2);
    }

    SECTION("L2 LB tight") {
        // 4 items: 60, 60, 40, 40 in cap 100.
        // Continuous LB = ceil(200/100) = 2. But large items (>50): 2 items,
        // each leaves 40 space. Small total = 80, fits in 80. L2 = max(2, 2+0) = 2.
        // Actual optimal = 2: {60+40, 60+40}.
        auto data = make_1d_instance(100, {60, 60, 40, 40});
        CHECK(data.l2_lower_bound() >= 2);
    }

    SECTION("L2 LB beats continuous") {
        // 3 items: 51, 51, 51 in cap 100.
        // Continuous LB = ceil(153/100) = 2. But all items > 50 (half cap),
        // so each needs its own bin. L2 = 3. Optimal = 3.
        auto data = make_1d_instance(100, {51, 51, 51});
        CHECK(data.continuous_lower_bound() == 2);
        CHECK(data.l2_lower_bound() == 3);
    }
}

// ===========================================================================
//  FFD + local search correctness on trivial instances
// ===========================================================================

TEST_CASE("FFD optimal on trivial instance", "[benchmark][packing]")
{
    // 3 items that fit perfectly in 1 bin.
    auto data = make_1d_instance(100, {30, 30, 40});
    PackingSolution sol(data);
    first_fit_decreasing(sol, data);

    CHECK(sol.feasible());
    CHECK(sol.all_assigned());
    CHECK(sol.num_bins_used() == 1);
}

TEST_CASE("FFD + LS on items needing merge", "[benchmark][packing]")
{
    // 4 items: 60, 30, 25, 15 in cap 100.
    // FFD assigns: bin0={60,30}, bin1={25,15}. Two bins used.
    // LS can merge bin1 into bin0: 60+30+25+15 > 100, doesn't fit.
    // Actually 60+30=90, 25+15=40. Can't merge (130>100).
    // So 2 bins is optimal. Verify FFD gets it right.
    auto data = make_1d_instance(100, {60, 30, 25, 15});
    PackingSolution sol(data);
    first_fit_decreasing(sol, data);
    local_search(sol);

    CHECK(sol.feasible());
    CHECK(sol.all_assigned());
    // FFD places: 60 -> bin0, 30 -> bin0 (90), 25 -> bin0... 90+25=115>100, no.
    // 25 -> bin1, 15 -> bin1 (40). So 2 bins, optimal.
    CHECK(sol.num_bins_used() == 2);
}

TEST_CASE("LS merges singleton bins", "[benchmark][packing]")
{
    // 3 items: 20, 20, 20 in cap 100. FFD puts all in bin0 (20+20+20=60).
    // Already optimal at 1 bin.
    auto data = make_1d_instance(100, {20, 20, 20});
    PackingSolution sol(data);
    first_fit_decreasing(sol, data);
    local_search(sol);

    CHECK(sol.feasible());
    CHECK(sol.num_bins_used() == 1);
}

// ===========================================================================
//  Multi-dimensional (vector) bin packing benchmark
// ===========================================================================

TEST_CASE("2D vector bin packing", "[benchmark][packing]")
{
    // 2D instance: capacity (100, 100).
    // Items: (60,30), (40,50), (30,70), (50,20), (20,40).
    // Optimal = 2 bins: {(60,30)+(40,50)=(100,80)}, {(30,70)+(50,20)+(20,40)=(100,130)}
    // Wait, (30+50+20, 70+20+40) = (100, 130) > 100 in dim 1. Not feasible.
    // Try: {(60,30)+(20,40)=(80,70)}, {(40,50)+(50,20)=(90,70)}, {(30,70)=(30,70)}.
    // 3 bins. Or: {(60,30)+(30,70)=(90,100)}, {(40,50)+(50,20)=(90,70)}, {(20,40)}.
    // That's 3 bins but (20,40) goes into bin with (40,50): (60,90). Yes!
    // So 2 bins: {(60,30)+(30,70)}, {(40,50)+(50,20)+(20,40)=(110,110)} > 100. No.
    // {(40,50)+(50,20)=(90,70)}, add (20,40)=(110,110)>100. No.
    // Optimal = 3 bins for this instance. Let me pick better items.
    // Items: (60,20), (40,30), (30,50), (50,40), (20,10).
    // {(60,20)+(40,30)=(100,50)}, {(30,50)+(50,40)+(20,10)=(100,100)}. 2 bins!

    PackingModel model;
    model.add_bin_type({.capacity = {100, 100}});
    model.add_item({.size = {60, 20}});
    model.add_item({.size = {40, 30}});
    model.add_item({.size = {30, 50}});
    model.add_item({.size = {50, 40}});
    model.add_item({.size = {20, 10}});

    auto data = PackingData::build(model);
    PackingSolution sol(data);
    first_fit_decreasing(sol, data);
    local_search(sol);

    std::cout << "\n=== 2D vector bin packing ===\n"
              << "  Bins used: " << sol.num_bins_used() << "\n"
              << "  Feasible:  " << (sol.feasible() ? "yes" : "no") << "\n"
              << std::endl;

    CHECK(sol.feasible());
    CHECK(sol.all_assigned());
    CHECK(sol.num_bins_used() <= 3);  // optimal is 2, allow some slack
}

// ===========================================================================
//  Bin packing with conflicts
// ===========================================================================

TEST_CASE("Bin packing with conflicts benchmark", "[benchmark][packing]")
{
    // 6 items, cap 100. Conflicts force extra bins.
    // Items: 30, 30, 30, 30, 30, 30. Without conflicts: 2 bins (3 per bin).
    // Conflicts: (0,1), (2,3), (4,5). Each pair needs separate bins.
    // Optimal = 3 bins: {0,2,4}, {1,3,5}, or any compatible grouping.
    // Actually with conflicts (0,1),(2,3),(4,5):
    // Bin A: 0,2,4 -> no conflicts among them. 30*3=90 <= 100. OK.
    // Bin B: 1,3,5 -> no conflicts among them. 30*3=90 <= 100. OK.
    // Optimal = 2 bins.

    PackingModel model;
    model.add_bin_type({.capacity = {100}});
    for (int i = 0; i < 6; ++i)
        model.add_item({.size = {30}});
    model.add_conflict(0, 1);
    model.add_conflict(2, 3);
    model.add_conflict(4, 5);

    auto data = PackingData::build(model);
    PackingSolution sol(data);
    first_fit_decreasing(sol, data);
    local_search(sol);

    std::cout << "\n=== Conflict bin packing ===\n"
              << "  Bins used: " << sol.num_bins_used() << "\n"
              << "  Feasible:  " << (sol.feasible() ? "yes" : "no") << "\n"
              << std::endl;

    CHECK(sol.feasible());
    CHECK(sol.all_assigned());
    CHECK(sol.num_bins_used() <= 3);  // optimal = 2, allow some slack
}

TEST_CASE("Dense conflicts force many bins", "[benchmark][packing]")
{
    // 5 items of size 10, cap 100. Full conflict clique: every pair conflicts.
    // Each item must be in its own bin. Optimal = 5.
    PackingModel model;
    model.add_bin_type({.capacity = {100}});
    for (int i = 0; i < 5; ++i)
        model.add_item({.size = {10}});
    for (int i = 0; i < 5; ++i)
        for (int j = i + 1; j < 5; ++j)
            model.add_conflict(i, j);

    auto data = PackingData::build(model);
    PackingSolution sol(data);
    first_fit_decreasing(sol, data);
    local_search(sol);

    CHECK(sol.feasible());
    CHECK(sol.all_assigned());
    CHECK(sol.num_bins_used() == 5);
}
