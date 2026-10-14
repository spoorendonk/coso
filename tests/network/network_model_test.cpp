#include "model/network_model.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

TEST_CASE("NetworkModel solves a simple minimum-cost flow", "[network][model]") {
    NetworkModel model;
    int s = model.add_node(5, "source");
    int m = model.add_node(0, "middle");
    int t = model.add_node(-5, "sink");

    int sm = model.add_arc(s, m, 1, 5);
    int mt = model.add_arc(m, t, 1, 5);
    int st = model.add_arc(s, t, 5, 5);

    Result result = model.solve(TimeLimit(1.0));

    REQUIRE(result.feasible());
    // Optimal route is s->m->t with 5 units: cost 10.
    REQUIRE(result.cost() == 10.0);
    // The arc flows are the solution itself, indexed as the arcs were declared.
    REQUIRE(result.arc_flows().size() == 1);
    auto const& flow = result.arc_flows()[0];
    REQUIRE(flow.size() == 3);
    REQUIRE(flow[sm] == 5.0);
    REQUIRE(flow[mt] == 5.0);
    REQUIRE(flow[st] == 0.0);
}

TEST_CASE("NetworkModel respects arc upper capacity", "[network][model]") {
    NetworkModel model;
    int s = model.add_node(4, "source");
    int t = model.add_node(-4, "sink");
    int cheap = model.add_arc(s, t, 1, 2);  // cheap, capped at 2
    int wide = model.add_arc(s, t, 5, 4);   // expensive, wide open

    Result result = model.solve(TimeLimit(1.0));

    REQUIRE(result.feasible());
    // The cap forces 2 units onto the expensive arc: 2 * 1 + 2 * 5.
    REQUIRE(result.cost() == 12.0);
    // Parallel arcs: only per-arc flows can say which one carries what.
    REQUIRE(result.arc_flows()[0][cheap] == 2.0);
    REQUIRE(result.arc_flows()[0][wide] == 2.0);
}

TEST_CASE("NetworkModel invalid arc indices throw", "[network][model]") {
    NetworkModel model;
    model.add_node(1, "s");
    model.add_node(-1, "t");

    REQUIRE_THROWS_AS(model.add_arc(0, 2, 1, 1), std::out_of_range);
    REQUIRE_THROWS_AS(model.add_arc(-1, 1, 1, 1), std::out_of_range);
}

TEST_CASE("NetworkModel deterministic work units repeat", "[network][model]") {
    NetworkModel model;
    int s = model.add_node(5, "s");
    int t = model.add_node(-5, "t");
    model.add_arc(s, t, 2, 5);

    Result r1 = model.solve(TimeLimit(1.0, 0.05));
    Result r2 = model.solve(TimeLimit(1.0, 0.05));

    REQUIRE(r1.work_ticks() > 0);
    REQUIRE(r1.work_ticks() == r2.work_ticks());
    REQUIRE(r1.work_units() == r2.work_units());
}
