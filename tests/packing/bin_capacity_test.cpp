#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "model/packing_model.h"
#include "packing/bin_capacity.h"
#include "packing/packing_data.h"
#include "packing/packing_solution.h"

using namespace coso;
using Catch::Matchers::WithinAbs;

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

static PackingData make_1d()
{
    PackingModel model;
    model.add_bin_type({.capacity = {10}});
    model.add_item({.size = {3}});  // 0
    model.add_item({.size = {5}});  // 1
    model.add_item({.size = {7}});  // 2
    model.add_item({.size = {2}});  // 3
    model.add_item({.size = {4}});  // 4
    return PackingData::build(model);
}

static PackingData make_2d()
{
    PackingModel model;
    model.add_bin_type({.capacity = {10, 20}});
    model.add_item({.size = {3, 8}});   // 0
    model.add_item({.size = {5, 10}});  // 1
    model.add_item({.size = {4, 7}});   // 2
    return PackingData::build(model);
}

static PackingData make_3d()
{
    PackingModel model;
    model.add_bin_type({.capacity = {10, 20, 30}});
    model.add_item({.size = {3, 8, 10}});   // 0
    model.add_item({.size = {5, 10, 15}});  // 1
    model.add_item({.size = {4, 7, 12}});   // 2
    return PackingData::build(model);
}

// ---------------------------------------------------------------------------
//  1D capacity tracking
// ---------------------------------------------------------------------------

TEST_CASE("BinCapacity: 1D add and remove items", "[packing][bin_capacity]")
{
    auto data = make_1d();
    PackingSolution sol(data);
    BinCapacity cap(sol);

    REQUIRE(cap.residual(0, 0) == 10);
    REQUIRE(cap.fits(0, 0));  // item 0, size 3

    cap.add_item(0, 0);  // bin 0 gets item 0 (size 3)
    REQUIRE(cap.residual(0, 0) == 7);
    REQUIRE(sol.bin_load(0, 0) == 3);

    cap.add_item(0, 1);  // bin 0 gets item 1 (size 5)
    REQUIRE(cap.residual(0, 0) == 2);
    REQUIRE(sol.bin_load(0, 0) == 8);

    // Item 2 (size 7) does not fit.
    REQUIRE_FALSE(cap.fits(0, 2));

    // Item 3 (size 2) fits exactly.
    REQUIRE(cap.fits(0, 3));

    cap.remove_item(0, 1);  // remove item 1
    REQUIRE(cap.residual(0, 0) == 7);
    REQUIRE(sol.bin_load(0, 0) == 3);
}

// ---------------------------------------------------------------------------
//  Multi-dimensional (2D) tracking
// ---------------------------------------------------------------------------

TEST_CASE("BinCapacity: 2D capacity tracking", "[packing][bin_capacity]")
{
    auto data = make_2d();
    PackingSolution sol(data);
    BinCapacity cap(sol);

    REQUIRE(cap.residual(0, 0) == 10);
    REQUIRE(cap.residual(0, 1) == 20);

    cap.add_item(0, 0);  // size {3, 8}
    REQUIRE(cap.residual(0, 0) == 7);
    REQUIRE(cap.residual(0, 1) == 12);

    // Item 1 {5, 10}: weight 3+5=8<=10, volume 8+10=18<=20 -> fits.
    REQUIRE(cap.fits(0, 1));

    cap.add_item(0, 1);  // bin 0: load {8, 18}
    REQUIRE(cap.residual(0, 0) == 2);
    REQUIRE(cap.residual(0, 1) == 2);

    // Item 2 {4, 7}: weight 8+4=12>10 -> does not fit.
    REQUIRE_FALSE(cap.fits(0, 2));

    // Put item 2 in bin 1 instead.
    cap.add_item(1, 2);  // size {4, 7}
    REQUIRE(cap.residual(1, 0) == 6);
    REQUIRE(cap.residual(1, 1) == 13);
}

// ---------------------------------------------------------------------------
//  3D capacity tracking
// ---------------------------------------------------------------------------

TEST_CASE("BinCapacity: 3D capacity tracking", "[packing][bin_capacity]")
{
    auto data = make_3d();
    PackingSolution sol(data);
    BinCapacity cap(sol);

    cap.add_item(0, 0);  // size {3, 8, 10}
    REQUIRE(cap.residual(0, 0) == 7);
    REQUIRE(cap.residual(0, 1) == 12);
    REQUIRE(cap.residual(0, 2) == 20);

    // Item 1 {5, 10, 15}: dim0 3+5=8<=10, dim1 8+10=18<=20, dim2 10+15=25<=30.
    REQUIRE(cap.fits(0, 1));

    cap.add_item(0, 1);
    REQUIRE(cap.residual(0, 0) == 2);
    REQUIRE(cap.residual(0, 1) == 2);
    REQUIRE(cap.residual(0, 2) == 5);

    // Item 2 {4, 7, 12}: dim0 8+4=12>10 -> does not fit.
    REQUIRE_FALSE(cap.fits(0, 2));
}

// ---------------------------------------------------------------------------
//  Best-fit heuristic
// ---------------------------------------------------------------------------

TEST_CASE("BinCapacity: best_fit finds tightest bin", "[packing][bin_capacity]")
{
    auto data = make_1d();
    PackingSolution sol(data);
    BinCapacity cap(sol);

    // Set up: bin 0 has load 7 (residual 3), bin 1 has load 5 (residual 5).
    cap.add_item(0, 2);  // item 2 size 7 -> bin 0
    cap.add_item(1, 1);  // item 1 size 5 -> bin 1

    // Item 3 (size 2): fits in both bins.
    // Bin 0 residual after = 3-2=1, bin 1 residual after = 5-2=3.
    // Best fit should pick bin 0 (tighter).
    int bf = cap.best_fit(3);
    REQUIRE(bf == 0);
}

TEST_CASE("BinCapacity: best_fit returns -1 when nothing fits", "[packing][bin_capacity]")
{
    PackingModel model;
    model.add_bin_type({.capacity = {5}, .count = 2});
    model.add_item({.size = {3}});  // 0
    model.add_item({.size = {3}});  // 1
    model.add_item({.size = {4}});  // 2

    auto data = PackingData::build(model);
    PackingSolution sol(data);
    BinCapacity cap(sol);

    cap.add_item(0, 0);  // bin 0: load 3, residual 2
    cap.add_item(1, 1);  // bin 1: load 3, residual 2

    // Item 2 (size 4) does not fit in either bin.
    REQUIRE(cap.best_fit(2) == -1);
}

TEST_CASE("BinCapacity: best_fit 2D picks tightest total residual", "[packing][bin_capacity]")
{
    auto data = make_2d();
    PackingSolution sol(data);
    BinCapacity cap(sol);

    // Bin 0: item 0 {3,8} -> residual {7, 12}, total = 19.
    cap.add_item(0, 0);

    // Bin 1 empty -> residual {10, 20}, total = 30.

    // Item 2 {4, 7}: fits in both bins.
    // Bin 0 after: residual {3, 5} = 8.
    // Bin 1 after: residual {6, 13} = 19.
    // Best fit = bin 0.
    REQUIRE(cap.best_fit(2) == 0);
}

// ---------------------------------------------------------------------------
//  First-fit heuristic
// ---------------------------------------------------------------------------

TEST_CASE("BinCapacity: first_fit returns first feasible bin", "[packing][bin_capacity]")
{
    auto data = make_1d();
    PackingSolution sol(data);
    BinCapacity cap(sol);

    // Fill bin 0 so item 2 (size 7) won't fit.
    cap.add_item(0, 1);  // item 1 size 5 -> bin 0 residual 5

    // Item 2 (size 7) doesn't fit in bin 0, should find bin 1 (empty, cap 10).
    int ff = cap.first_fit(2);
    REQUIRE(ff == 1);
}

TEST_CASE("BinCapacity: first_fit returns -1 when nothing fits", "[packing][bin_capacity]")
{
    PackingModel model;
    model.add_bin_type({.capacity = {3}, .count = 1});
    model.add_item({.size = {5}});  // too big for any bin

    auto data = PackingData::build(model);
    PackingSolution sol(data);
    BinCapacity cap(sol);

    REQUIRE(cap.first_fit(0) == -1);
}

TEST_CASE("BinCapacity: first_fit on empty bins returns bin 0", "[packing][bin_capacity]")
{
    auto data = make_1d();
    PackingSolution sol(data);
    BinCapacity cap(sol);

    // All bins empty; first fit for any item is bin 0.
    REQUIRE(cap.first_fit(0) == 0);
    REQUIRE(cap.first_fit(2) == 0);
}

// ---------------------------------------------------------------------------
//  Lower bound
// ---------------------------------------------------------------------------

TEST_CASE("BinCapacity: continuous lower bound 1D", "[packing][bin_capacity]")
{
    auto data = make_1d();
    PackingSolution sol(data);
    BinCapacity cap(sol);

    // Items: 3+5+7+2+4 = 21, cap = 10. ceil(21/10) = 3.
    REQUIRE(cap.continuous_lower_bound() == 3);
}

TEST_CASE("BinCapacity: continuous lower bound 2D", "[packing][bin_capacity]")
{
    auto data = make_2d();
    PackingSolution sol(data);
    BinCapacity cap(sol);

    // Dim 0: 3+5+4=12, cap 10 -> ceil(12/10) = 2.
    // Dim 1: 8+10+7=25, cap 20 -> ceil(25/20) = 2.
    // Max = 2.
    REQUIRE(cap.continuous_lower_bound() == 2);
}

TEST_CASE("BinCapacity: continuous lower bound exact", "[packing][bin_capacity]")
{
    PackingModel model;
    model.add_bin_type({.capacity = {10}});
    model.add_item({.size = {10}});
    model.add_item({.size = {10}});
    model.add_item({.size = {10}});

    auto data = PackingData::build(model);
    PackingSolution sol(data);
    BinCapacity cap(sol);

    // 30 / 10 = 3 exactly.
    REQUIRE(cap.continuous_lower_bound() == 3);
}

// ---------------------------------------------------------------------------
//  Utilization
// ---------------------------------------------------------------------------

TEST_CASE("BinCapacity: utilization of empty bin is 0", "[packing][bin_capacity]")
{
    auto data = make_1d();
    PackingSolution sol(data);
    BinCapacity cap(sol);

    REQUIRE_THAT(cap.utilization(0), WithinAbs(0.0, 1e-9));
}

TEST_CASE("BinCapacity: utilization 1D", "[packing][bin_capacity]")
{
    auto data = make_1d();
    PackingSolution sol(data);
    BinCapacity cap(sol);

    cap.add_item(0, 2);  // item 2, size 7, cap 10 -> 70%
    REQUIRE_THAT(cap.utilization(0), WithinAbs(0.7, 1e-9));

    cap.add_item(0, 0);  // item 0, size 3 -> load 10, 100%
    REQUIRE_THAT(cap.utilization(0), WithinAbs(1.0, 1e-9));
}

TEST_CASE("BinCapacity: utilization 2D", "[packing][bin_capacity]")
{
    auto data = make_2d();
    PackingSolution sol(data);
    BinCapacity cap(sol);

    cap.add_item(0, 0);  // size {3, 8}, cap {10, 20}
    // dim 0: 3/10 = 0.3, dim 1: 8/20 = 0.4. Average = 0.35.
    REQUIRE_THAT(cap.utilization(0), WithinAbs(0.35, 1e-9));
}

TEST_CASE("BinCapacity: total utilization", "[packing][bin_capacity]")
{
    auto data = make_1d();
    PackingSolution sol(data);
    BinCapacity cap(sol);

    cap.add_item(0, 2);  // bin 0: load 7/10 = 0.7
    cap.add_item(1, 1);  // bin 1: load 5/10 = 0.5

    // Average utilization of used bins: (0.7 + 0.5) / 2 = 0.6.
    REQUIRE_THAT(cap.total_utilization(), WithinAbs(0.6, 1e-9));
}

TEST_CASE("BinCapacity: total utilization with no items is 0", "[packing][bin_capacity]")
{
    auto data = make_1d();
    PackingSolution sol(data);
    BinCapacity cap(sol);

    REQUIRE_THAT(cap.total_utilization(), WithinAbs(0.0, 1e-9));
}
