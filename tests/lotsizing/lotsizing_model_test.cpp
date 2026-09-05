#include "model/lotsizing_model.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>

using namespace coso;
using Catch::Matchers::WithinAbs;

namespace {

/// The 2 x 3 CLSP instance used by the capacity test below.
///
///   product   setup_cost  setup_time  unit_production_cost  holding_cost
///   A (0)         50        2 * s              1                 1
///   B (1)         60        3 * s              2                 1
///
/// Demand is 10 per product per period; periods 1 and 2 have slack capacity.
/// `setup_time_scale` is 1 for the declared setup times and 0 for the control
/// that removes them and changes nothing else.
LotSizingModel make_clsp(double capacity_period_0, double setup_time_scale) {
    LotSizingModel model;
    model.set_num_periods(3);
    int a = model.add_product(50.0, 2.0 * setup_time_scale, 1.0, 1.0);
    int b = model.add_product(60.0, 3.0 * setup_time_scale, 2.0, 1.0);
    for (int t = 0; t < 3; ++t) {
        model.set_demand(a, t, 10.0);
        model.set_demand(b, t, 10.0);
    }
    model.set_capacity(0, capacity_period_0);
    model.set_capacity(1, 100.0);
    model.set_capacity(2, 100.0);
    return model;
}

/// Capacity consumed in period `t` by the *returned* production plan, priced
/// with the declared setup times: one unit of capacity per unit produced, plus
/// the product's setup time wherever it produces at all.
double period_usage(Result const& r, std::vector<double> const& setup_time, int t) {
    double usage = 0.0;
    for (size_t p = 0; p < r.production().size(); ++p) {
        double qty = r.production()[p][static_cast<size_t>(t)];
        usage += qty;
        if (qty > 0.0) {
            usage += setup_time[p];
        }
    }
    return usage;
}

/// End-of-period inventory implied by the *returned* production and the
/// declared demand, starting from zero: the balance the model promises.
std::vector<double> implied_inventory(Result const& r, size_t product,
                                      std::vector<double> const& demand) {
    std::vector<double> inv;
    double carried = 0.0;
    for (size_t t = 0; t < demand.size(); ++t) {
        carried += r.production()[product][t] - demand[t];
        inv.push_back(carried);
    }
    return inv;
}

}  // namespace

TEST_CASE("LotSizingModel solves basic CLSP instance", "[lotsizing][model]") {
    LotSizingModel model;
    model.set_num_periods(4);
    int p = model.add_product(100.0, 2.0, 1.0, 2.0);

    model.set_demand(p, 0, 10.0);
    model.set_demand(p, 1, 20.0);
    model.set_demand(p, 2, 15.0);
    model.set_demand(p, 3, 25.0);
    for (int t = 0; t < 4; ++t) {
        model.set_capacity(t, 80.0);
    }

    Result result = model.solve(TimeLimit(1.0));
    REQUIRE(result.feasible());
    REQUIRE(result.production().size() == 1);
    REQUIRE(result.production()[0].size() == 4);
    REQUIRE(result.inventory().size() == 1);
    REQUIRE(result.cost() > 0.0);
}

TEST_CASE("LotSizingModel accepts a BOM declaration", "[lotsizing][model]") {
    LotSizingModel model;
    model.set_num_periods(3);
    int parent = model.add_product(120.0, 2.0, 1.0, 2.0);
    int child = model.add_product(80.0, 1.0, 0.5, 1.5);

    model.add_bom(parent, child, 2.0);
    model.set_demand(parent, 0, 5.0);
    model.set_demand(parent, 1, 6.0);
    model.set_demand(parent, 2, 7.0);
    for (int t = 0; t < 3; ++t) {
        model.set_capacity(t, 100.0);
    }

    Result result = model.solve(TimeLimit(1.0));
    REQUIRE(result.production().size() == 2);
    REQUIRE(result.inventory().size() == 2);
}

TEST_CASE("LotSizingModel validates indices", "[lotsizing][model]") {
    LotSizingModel model;
    model.set_num_periods(2);
    int p = model.add_product(50.0, 1.0, 1.0, 1.0);
    (void)p;

    REQUIRE_THROWS_AS(model.set_demand(2, 0, 1.0), std::out_of_range);
    REQUIRE_THROWS_AS(model.set_demand(0, 2, 1.0), std::out_of_range);
    REQUIRE_THROWS_AS(model.set_capacity(3, 5.0), std::out_of_range);
    REQUIRE_THROWS_AS(model.add_bom(0, 1, 1.0), std::out_of_range);
}

// ---------------------------------------------------------------------------
//  CLSP with a binding capacity
// ---------------------------------------------------------------------------

TEST_CASE("LotSizingModel: CLSP with capacity binding in one period", "[lotsizing][model]") {
    // Two products, three periods, demand 10 per product per period, so total
    // production is fixed at 30 per product: there is no initial inventory, no
    // backlog, and overproduction only costs. The variable production cost is
    // therefore the constant 30 * 1 + 30 * 2 = 90, and the plan is decided by
    // setups and holding alone.
    //
    // Uncapacitated optimum: one setup per product in period 0.
    //   setups 50 + 60 = 110, holding (20 + 10) * 1 twice = 60, production 90
    //   -> 260, needing 30 + 30 + 2 + 3 = 65 units of capacity in period 0.
    //
    // Capacity 25 in period 0 rules that out. The optimum becomes
    //   A = (10, 20, 0), B = (10, 20, 0)
    //   setups 2 * 50 + 2 * 60 = 220, holding 10 + 10 = 20, production 90 -> 330,
    // and period-0 usage is 10 + 10 + 2 + 3 = 25, exactly the declared capacity.
    // Unique: verified by exhaustive enumeration over a 0.25-unit production
    // grid, which finds one optimal plan at 330 and one at 260 uncapacitated.
    std::vector<double> const demand = {10.0, 10.0, 10.0};
    std::vector<double> const setup_time = {2.0, 3.0};

    SECTION("the capacitated optimum, checked against the declaration") {
        LotSizingModel model = make_clsp(25.0, 1.0);
        Result result = model.solve(TimeLimit(1.0));

        REQUIRE(result.feasible());
        REQUIRE(result.production().size() == 2);
        REQUIRE(result.inventory().size() == 2);

        std::vector<std::vector<double>> const expected_production = {{10.0, 20.0, 0.0},
                                                                      {10.0, 20.0, 0.0}};
        std::vector<std::vector<double>> const expected_inventory = {{0.0, 10.0, 0.0},
                                                                     {0.0, 10.0, 0.0}};
        for (size_t p = 0; p < 2; ++p) {
            REQUIRE(result.production()[p].size() == 3);
            REQUIRE(result.inventory()[p].size() == 3);
            for (size_t t = 0; t < 3; ++t) {
                CHECK_THAT(result.production()[p][t], WithinAbs(expected_production[p][t], 1e-9));
                CHECK_THAT(result.inventory()[p][t], WithinAbs(expected_inventory[p][t], 1e-9));
            }
            // The returned inventory is the balance the returned production and
            // the declared demand imply, not an independent number.
            std::vector<double> const implied = implied_inventory(result, p, demand);
            for (size_t t = 0; t < 3; ++t) {
                CHECK_THAT(result.inventory()[p][t], WithinAbs(implied[t], 1e-9));
                CHECK(result.inventory()[p][t] >= -1e-9);
            }
        }

        // Capacity, recomputed from the returned plan and the declared setup
        // times: binding in period 0 and slack everywhere else.
        CHECK_THAT(period_usage(result, setup_time, 0), WithinAbs(25.0, 1e-9));
        CHECK_THAT(period_usage(result, setup_time, 1), WithinAbs(45.0, 1e-9));
        CHECK_THAT(period_usage(result, setup_time, 2), WithinAbs(0.0, 1e-9));
        CHECK(period_usage(result, setup_time, 1) < 100.0);
        CHECK(period_usage(result, setup_time, 2) < 100.0);

        CHECK_THAT(result.cost(), WithinAbs(330.0, 1e-9));
    }

    SECTION("control: relaxing the capacity changes the answer") {
        // Same declaration with capacity 1000 in period 0. If capacity were
        // ignored this is what the section above would return too.
        LotSizingModel model = make_clsp(1000.0, 1.0);
        Result result = model.solve(TimeLimit(1.0));

        REQUIRE(result.feasible());
        std::vector<std::vector<double>> const expected_production = {{30.0, 0.0, 0.0},
                                                                      {30.0, 0.0, 0.0}};
        for (size_t p = 0; p < 2; ++p) {
            for (size_t t = 0; t < 3; ++t) {
                CHECK_THAT(result.production()[p][t], WithinAbs(expected_production[p][t], 1e-9));
            }
        }
        CHECK_THAT(period_usage(result, setup_time, 0), WithinAbs(65.0, 1e-9));
        CHECK_THAT(result.cost(), WithinAbs(260.0, 1e-9));
    }

    SECTION("setup time consumes capacity") {
        // Capacity 20 in period 0. Both products carry demand 10 in period 0
        // and there is no initial inventory, so every feasible plan sets both
        // of them up there: 10 + 10 + 2 + 3 = 25 > 20. The instance is
        // infeasible, and the returned plan overruns the declared capacity.
        LotSizingModel tight = make_clsp(20.0, 1.0);
        Result tight_result = tight.solve(TimeLimit(1.0));
        CHECK_FALSE(tight_result.feasible());
        CHECK(period_usage(tight_result, setup_time, 0) > 20.0);

        // The same declaration with the setup times removed and nothing else
        // changed: 10 + 10 = 20 fits, and the capacitated optimum comes back.
        LotSizingModel free_setup = make_clsp(20.0, 0.0);
        Result free_result = free_setup.solve(TimeLimit(1.0));
        REQUIRE(free_result.feasible());
        std::vector<std::vector<double>> const expected_production = {{10.0, 20.0, 0.0},
                                                                      {10.0, 20.0, 0.0}};
        for (size_t p = 0; p < 2; ++p) {
            for (size_t t = 0; t < 3; ++t) {
                CHECK_THAT(free_result.production()[p][t],
                           WithinAbs(expected_production[p][t], 1e-9));
            }
        }
        CHECK_THAT(period_usage(free_result, {0.0, 0.0}, 0), WithinAbs(20.0, 1e-9));
        CHECK_THAT(free_result.cost(), WithinAbs(330.0, 1e-9));
    }
}

TEST_CASE("LotSizingModel: BOM generates dependent demand for the child", "[lotsizing][model]") {
    SKIP(
        "add_bom() is stored, copied into LotsizingData and never read again: "
        "LotsizingSolution::recompute_inventory_ balances external demand only, and neither "
        "the constructions nor the operators mention the BOM, so a component with no external "
        "demand is never produced and the result is reported feasible — coso#210");

    LotSizingModel model;
    model.set_num_periods(3);
    int parent = model.add_product(120.0, 2.0, 1.0, 2.0);
    int child = model.add_product(80.0, 1.0, 0.5, 1.5);

    model.add_bom(parent, child, 2.0);
    for (int t = 0; t < 3; ++t) {
        model.set_demand(parent, t, 5.0);
        model.set_capacity(t, 1000.0);
    }

    Result result = model.solve(TimeLimit(1.0));

    REQUIRE(result.feasible());
    double parent_total = 0.0;
    double child_total = 0.0;
    for (int t = 0; t < 3; ++t) {
        parent_total += result.production()[0][static_cast<size_t>(t)];
        child_total += result.production()[1][static_cast<size_t>(t)];
    }
    CHECK_THAT(parent_total, WithinAbs(15.0, 1e-9));
    // Two units of child per unit of parent, and no external demand for it.
    CHECK_THAT(child_total, WithinAbs(2.0 * parent_total, 1e-9));
}
