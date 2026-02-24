#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "lotsizing/lotsizing_data.h"
#include "lotsizing/lotsizing_solution.h"
#include "lotsizing/lotsizing_operators.h"
#include "lotsizing/construction.h"

using Catch::Matchers::WithinAbs;

// ---------------------------------------------------------------------------
//  Test helpers
// ---------------------------------------------------------------------------

namespace {

/// Build a simple single-product, 4-period CLSP instance.
///
///   Product 0: setup_cost=100, setup_time=2, unit_cost=1, holding_cost=2
///   Demand: [10, 20, 15, 25]  (total = 70)
///   Capacity per period: 50
coso::LotsizingData make_simple_instance()
{
    coso::LotsizingData::Builder b;
    b.set_num_periods(4);
    int p = b.add_product(100.0, 2.0, 1.0, 2.0);
    b.set_demand(p, 0, 10.0);
    b.set_demand(p, 1, 20.0);
    b.set_demand(p, 2, 15.0);
    b.set_demand(p, 3, 25.0);
    for (int t = 0; t < 4; ++t)
        b.set_capacity(t, 50.0);
    return b.build();
}

/// Build a 2-product, 3-period instance.
coso::LotsizingData make_multi_product_instance()
{
    coso::LotsizingData::Builder b;
    b.set_num_periods(3);
    int p0 = b.add_product(80.0, 1.0, 0.5, 1.5);
    int p1 = b.add_product(120.0, 3.0, 1.0, 2.0);
    b.set_demand(p0, 0, 10.0);
    b.set_demand(p0, 1, 15.0);
    b.set_demand(p0, 2, 20.0);
    b.set_demand(p1, 0, 5.0);
    b.set_demand(p1, 1, 10.0);
    b.set_demand(p1, 2, 8.0);
    for (int t = 0; t < 3; ++t)
        b.set_capacity(t, 60.0);
    return b.build();
}

/// Build a 2-level MLCLSP: product 0 (end) requires 2 units of product 1.
coso::LotsizingData make_mlclsp_instance()
{
    coso::LotsizingData::Builder b;
    b.set_num_periods(3);
    int p0 = b.add_product(100.0, 2.0, 1.0, 3.0);  // end product
    int p1 = b.add_product(50.0, 1.0, 0.5, 1.0);   // component
    b.set_demand(p0, 0, 5.0);
    b.set_demand(p0, 1, 10.0);
    b.set_demand(p0, 2, 8.0);
    // Component has no external demand; only dependent demand via BOM.
    b.add_bom(p0, p1, 2.0);
    for (int t = 0; t < 3; ++t)
        b.set_capacity(t, 100.0);
    return b.build();
}

} // anonymous namespace

// ===========================================================================
//  LotsizingData tests
// ===========================================================================

TEST_CASE("LotsizingData: basic construction", "[lotsizing]")
{
    auto data = make_simple_instance();

    CHECK(data.num_products() == 1);
    CHECK(data.num_periods() == 4);
    CHECK_THAT(data.demand(0, 0), WithinAbs(10.0, 1e-9));
    CHECK_THAT(data.demand(0, 1), WithinAbs(20.0, 1e-9));
    CHECK_THAT(data.demand(0, 2), WithinAbs(15.0, 1e-9));
    CHECK_THAT(data.demand(0, 3), WithinAbs(25.0, 1e-9));
    CHECK_THAT(data.total_demand(0), WithinAbs(70.0, 1e-9));
    CHECK_THAT(data.capacity(0), WithinAbs(50.0, 1e-9));
    CHECK_THAT(data.setup_cost(0), WithinAbs(100.0, 1e-9));
    CHECK_THAT(data.setup_time(0), WithinAbs(2.0, 1e-9));
    CHECK_THAT(data.unit_production_cost(0), WithinAbs(1.0, 1e-9));
    CHECK_THAT(data.holding_cost(0), WithinAbs(2.0, 1e-9));
}

TEST_CASE("LotsizingData: multi-product", "[lotsizing]")
{
    auto data = make_multi_product_instance();

    CHECK(data.num_products() == 2);
    CHECK(data.num_periods() == 3);
    CHECK_THAT(data.demand(1, 2), WithinAbs(8.0, 1e-9));
    CHECK_THAT(data.setup_cost(1), WithinAbs(120.0, 1e-9));
}

TEST_CASE("LotsizingData: MLCLSP BOM", "[lotsizing]")
{
    auto data = make_mlclsp_instance();

    CHECK(data.is_multi_level());
    CHECK(data.num_bom_entries() == 1);
    CHECK(data.is_end_product(0));
    CHECK_FALSE(data.is_end_product(1));

    auto children = data.children(0);
    REQUIRE(children.size() == 1);
    CHECK(children[0].first == 1);
    CHECK_THAT(children[0].second, WithinAbs(2.0, 1e-9));

    auto parents = data.parents(1);
    REQUIRE(parents.size() == 1);
    CHECK(parents[0].first == 0);
}

// ===========================================================================
//  LotsizingSolution tests
// ===========================================================================

TEST_CASE("LotsizingSolution: empty solution has zero production", "[lotsizing]")
{
    auto data = make_simple_instance();
    coso::LotsizingSolution sol(data);

    CHECK_THAT(sol.production(0, 0), WithinAbs(0.0, 1e-9));
    CHECK_THAT(sol.production(0, 3), WithinAbs(0.0, 1e-9));
    CHECK_FALSE(sol.setup(0, 0));
}

TEST_CASE("LotsizingSolution: empty solution has demand violation", "[lotsizing]")
{
    auto data = make_simple_instance();
    coso::LotsizingSolution sol(data);

    CHECK(sol.has_demand_violation());
    CHECK_FALSE(sol.feasible());
    CHECK(sol.total_backlog() > 0.0);
}

TEST_CASE("LotsizingSolution: set production updates inventory", "[lotsizing]")
{
    auto data = make_simple_instance();
    coso::LotsizingSolution sol(data);

    // Produce all demand in period 0.
    sol.set_production(0, 0, 70.0);

    // inventory[0] = 0 + 70 - 10 = 60
    CHECK_THAT(sol.inventory(0, 0), WithinAbs(60.0, 1e-9));
    // inventory[1] = 60 - 20 = 40
    CHECK_THAT(sol.inventory(0, 1), WithinAbs(40.0, 1e-9));
    // inventory[2] = 40 - 15 = 25
    CHECK_THAT(sol.inventory(0, 2), WithinAbs(25.0, 1e-9));
    // inventory[3] = 25 - 25 = 0
    CHECK_THAT(sol.inventory(0, 3), WithinAbs(0.0, 1e-9));

    CHECK(sol.setup(0, 0));
    CHECK_FALSE(sol.has_demand_violation());
}

TEST_CASE("LotsizingSolution: cost computation", "[lotsizing]")
{
    auto data = make_simple_instance();
    coso::LotsizingSolution sol(data);

    // Lot-for-lot: produce demand in each period.
    sol.set_production(0, 0, 10.0);
    sol.set_production(0, 1, 20.0);
    sol.set_production(0, 2, 15.0);
    sol.set_production(0, 3, 25.0);

    // 4 setups * 100 = 400 setup cost.
    CHECK_THAT(sol.setup_cost(), WithinAbs(400.0, 1e-9));
    // No inventory carried, so holding cost = 0.
    CHECK_THAT(sol.holding_cost(), WithinAbs(0.0, 1e-9));
    // Production cost = 70 * 1 = 70.
    CHECK_THAT(sol.production_cost(), WithinAbs(70.0, 1e-9));
    CHECK_THAT(sol.cost(), WithinAbs(470.0, 1e-9));
    CHECK(sol.feasible());
}

TEST_CASE("LotsizingSolution: capacity violation detection", "[lotsizing]")
{
    auto data = make_simple_instance();
    coso::LotsizingSolution sol(data);

    // Produce 70 units in period 0 (capacity = 50).
    // usage = 70 (production) + 2 (setup) = 72 > 50
    sol.set_production(0, 0, 70.0);
    CHECK(sol.has_capacity_violation());
    CHECK_FALSE(sol.feasible());
}

TEST_CASE("LotsizingSolution: feasible when spread across periods", "[lotsizing]")
{
    auto data = make_simple_instance();
    coso::LotsizingSolution sol(data);

    // capacity = 50, setup_time = 2, so max production per period = 48.
    // Spread: produce 30 in t=0 (covers t=0 demand 10 + t=1 demand 20),
    //         produce 40 in t=2 (covers t=2 demand 15 + t=3 demand 25).
    sol.set_production(0, 0, 30.0);
    sol.set_production(0, 2, 40.0);

    CHECK_FALSE(sol.has_demand_violation());
    CHECK_FALSE(sol.has_capacity_violation());
    CHECK(sol.feasible());
}

// ===========================================================================
//  Construction heuristic tests
// ===========================================================================

TEST_CASE("lot_for_lot produces demand each period", "[lotsizing]")
{
    auto data = make_simple_instance();
    auto sol = coso::lot_for_lot(data);

    CHECK(sol.feasible());
    CHECK_THAT(sol.production(0, 0), WithinAbs(10.0, 1e-9));
    CHECK_THAT(sol.production(0, 1), WithinAbs(20.0, 1e-9));
    CHECK_THAT(sol.production(0, 2), WithinAbs(15.0, 1e-9));
    CHECK_THAT(sol.production(0, 3), WithinAbs(25.0, 1e-9));
    // All inventory should be 0.
    for (int t = 0; t < 4; ++t)
        CHECK_THAT(sol.inventory(0, t), WithinAbs(0.0, 1e-9));
}

TEST_CASE("silver_meal produces feasible solution", "[lotsizing]")
{
    auto data = make_simple_instance();
    auto sol = coso::silver_meal(data);

    CHECK_FALSE(sol.has_demand_violation());
    // Silver-Meal should consolidate some lots (fewer setups).
    int setups = 0;
    for (int t = 0; t < data.num_periods(); ++t)
        if (sol.setup(0, t)) ++setups;
    // Must have at least 1 setup and at most 4.
    CHECK(setups >= 1);
    CHECK(setups <= 4);
}

TEST_CASE("silver_meal cost <= lot_for_lot cost", "[lotsizing]")
{
    auto data = make_simple_instance();
    auto lfl = coso::lot_for_lot(data);
    auto sm  = coso::silver_meal(data);

    CHECK(sm.cost() <= lfl.cost() + 1e-9);
}

TEST_CASE("part_period_balancing produces feasible solution", "[lotsizing]")
{
    auto data = make_simple_instance();
    auto sol = coso::part_period_balancing(data);

    CHECK_FALSE(sol.has_demand_violation());
}

TEST_CASE("part_period_balancing cost <= lot_for_lot cost", "[lotsizing]")
{
    auto data = make_simple_instance();
    auto lfl = coso::lot_for_lot(data);
    auto ppb = coso::part_period_balancing(data);

    CHECK(ppb.cost() <= lfl.cost() + 1e-9);
}

TEST_CASE("construction heuristics work for multi-product", "[lotsizing]")
{
    auto data = make_multi_product_instance();

    auto lfl = coso::lot_for_lot(data);
    CHECK_FALSE(lfl.has_demand_violation());

    auto sm = coso::silver_meal(data);
    CHECK_FALSE(sm.has_demand_violation());

    auto ppb = coso::part_period_balancing(data);
    CHECK_FALSE(ppb.has_demand_violation());
}

// ===========================================================================
//  Operator tests
// ===========================================================================

TEST_CASE("ShiftProduction: shift production to earlier period", "[lotsizing]")
{
    auto data = make_simple_instance();
    auto sol = coso::lot_for_lot(data);

    // Shift production from period 1 (20 units) to period 0.
    auto shift = coso::evaluate_shift(sol, 0, 1, 0, 20.0);
    CHECK(shift.product == 0);
    CHECK(shift.from_period == 1);
    CHECK(shift.to_period == 0);
    CHECK_THAT(shift.quantity, WithinAbs(20.0, 1e-9));

    // Shifting earlier: saves a setup (-100) but adds holding cost (+2*20*1 = 40).
    // Net delta = -100 + 40 = -60.
    CHECK_THAT(shift.delta, WithinAbs(-60.0, 1e-9));
}

TEST_CASE("ShiftProduction: apply improves cost", "[lotsizing]")
{
    auto data = make_simple_instance();
    auto sol = coso::lot_for_lot(data);
    double old_cost = sol.cost();

    auto shift = coso::evaluate_shift(sol, 0, 1, 0, 20.0);
    REQUIRE(coso::is_feasible(sol, shift));

    coso::apply(sol, shift);
    CHECK(sol.cost() < old_cost);
    CHECK_FALSE(sol.has_demand_violation());
}

TEST_CASE("MergeSetups: merge adjacent setups", "[lotsizing]")
{
    auto data = make_simple_instance();
    auto sol = coso::lot_for_lot(data);

    // Merge period 1's production (20) into period 0.
    auto merge = coso::evaluate_merge(sol, 0, 1, 0);
    CHECK(merge.source_period == 1);
    CHECK(merge.target_period == 0);
    // Should save one setup.
    CHECK(merge.delta < 0.0);
}

TEST_CASE("MergeSetups: apply eliminates a setup", "[lotsizing]")
{
    auto data = make_simple_instance();
    auto sol = coso::lot_for_lot(data);

    auto merge = coso::evaluate_merge(sol, 0, 1, 0);
    REQUIRE(coso::is_feasible(sol, merge));

    coso::apply(sol, merge);
    CHECK_FALSE(sol.setup(0, 1));
    CHECK(sol.setup(0, 0));
    CHECK_THAT(sol.production(0, 0), WithinAbs(30.0, 1e-9));
    CHECK_THAT(sol.production(0, 1), WithinAbs(0.0, 1e-9));
    CHECK_FALSE(sol.has_demand_violation());
}

TEST_CASE("SplitLot: split overloaded period", "[lotsizing]")
{
    // Create a tight instance where one period is overloaded.
    coso::LotsizingData::Builder b;
    b.set_num_periods(2);
    int p = b.add_product(10.0, 5.0, 1.0, 1.0);
    b.set_demand(p, 0, 0.0);
    b.set_demand(p, 1, 40.0);
    b.set_capacity(0, 50.0);
    b.set_capacity(1, 50.0);
    auto data = b.build();

    coso::LotsizingSolution sol(data);
    // All production in period 1: 40 + 5 (setup) = 45 <= 50, actually fits.
    sol.set_production(0, 1, 40.0);
    CHECK(sol.feasible());

    // But if we increase production to 50 it won't fit.
    sol.set_production(0, 1, 50.0);
    // 50 + 5 = 55 > 50
    CHECK(sol.has_capacity_violation());

    // Split 20 units to period 0.
    auto split = coso::evaluate_split(sol, 0, 1, 0, 20.0);
    CHECK_THAT(split.quantity, WithinAbs(20.0, 1e-9));

    coso::apply(sol, split);
    CHECK_THAT(sol.production(0, 1), WithinAbs(30.0, 1e-9));
    CHECK_THAT(sol.production(0, 0), WithinAbs(20.0, 1e-9));
}

TEST_CASE("enumerate_shifts returns improving moves", "[lotsizing]")
{
    auto data = make_simple_instance();
    auto sol = coso::lot_for_lot(data);

    auto shifts = coso::enumerate_shifts(sol);
    CHECK_FALSE(shifts.empty());

    // At least one should be improving (negative delta).
    bool has_improving = false;
    for (auto const& s : shifts)
        if (s.delta < -1e-9) has_improving = true;
    CHECK(has_improving);
}

TEST_CASE("enumerate_merges returns valid moves", "[lotsizing]")
{
    auto data = make_simple_instance();
    auto sol = coso::lot_for_lot(data);

    auto merges = coso::enumerate_merges(sol);
    CHECK_FALSE(merges.empty());

    // All enumerated moves should be feasible.
    for (auto const& m : merges) {
        CHECK(coso::is_feasible(sol, m));
    }
}
