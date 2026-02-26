#include <catch2/catch_test_macros.hpp>

#include "model/network_model.h"

using namespace coso;

TEST_CASE("NetworkModel solves a simple minimum-cost flow", "[network][model]")
{
    NetworkModel model;
    int s = model.add_node(5, "source");
    int m = model.add_node(0, "middle");
    int t = model.add_node(-5, "sink");

    model.add_arc(s, m, 1, 0, 5);
    model.add_arc(m, t, 1, 0, 5);
    model.add_arc(s, t, 5, 0, 5);

    Result result = model.solve(TimeLimit(1.0));

    REQUIRE(result.feasible());
    // Optimal route is s->m->t with 5 units: cost 10.
    REQUIRE(result.cost() == 10.0);
    REQUIRE(result.flows().size() == 1);
    REQUIRE_FALSE(result.flows()[0].empty());
}

TEST_CASE("NetworkModel invalid arc indices throw", "[network][model]")
{
    NetworkModel model;
    model.add_node(1, "s");
    model.add_node(-1, "t");

    REQUIRE_THROWS_AS(model.add_arc(0, 2, 1, 0, 1), std::out_of_range);
    REQUIRE_THROWS_AS(model.add_arc(-1, 1, 1, 0, 1), std::out_of_range);
}

TEST_CASE("NetworkModel deterministic work units repeat", "[network][model]")
{
    NetworkModel model;
    int s = model.add_node(5, "s");
    int t = model.add_node(-5, "t");
    model.add_arc(s, t, 2, 0, 5);

    Result r1 = model.solve(TimeLimit(1.0, 0.05));
    Result r2 = model.solve(TimeLimit(1.0, 0.05));

    REQUIRE(r1.work_ticks() > 0);
    REQUIRE(r1.work_ticks() == r2.work_ticks());
    REQUIRE(r1.work_units() == r2.work_units());
}
