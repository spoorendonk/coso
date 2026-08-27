#include "model/lotsizing_model.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

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

TEST_CASE("LotSizingModel supports BOM", "[lotsizing][model]") {
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
