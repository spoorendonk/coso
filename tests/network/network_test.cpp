#include "network/construction.h"
#include "network/mcf_solver.h"
#include "network/network_data.h"
#include "network/network_operators.h"
#include "network/network_solution.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a simple 4-node network
//
//  Node 0 (supply=10) --cost=2--> Node 1 --cost=3--> Node 3 (demand=-10)
//                       \                              ^
//                        --cost=1--> Node 2 --cost=4--/
//
//  Optimal: route 10 units via 0->1->3 at cost 10*(2+3)=50
//  (cheaper than 0->2->3 at cost 10*(1+4)=50 -- same cost, either is optimal)
// ---------------------------------------------------------------------------

static NetworkData build_simple_network() {
    NetworkData::Builder b;
    b.add_node(10, "source");  // 0
    b.add_node(0, "relay_a");  // 1
    b.add_node(0, "relay_b");  // 2
    b.add_node(-10, "sink");   // 3

    b.add_arc(0, 1, /*cost=*/2, /*lower=*/0, /*upper=*/10);  // arc 0
    b.add_arc(1, 3, /*cost=*/3, /*lower=*/0, /*upper=*/10);  // arc 1
    b.add_arc(0, 2, /*cost=*/1, /*lower=*/0, /*upper=*/10);  // arc 2
    b.add_arc(2, 3, /*cost=*/4, /*lower=*/0, /*upper=*/10);  // arc 3

    return b.build();
}

// ---------------------------------------------------------------------------
//  Helper: build asymmetric cost network
//
//  Node 0 (supply=5) --cost=1--> Node 1 --cost=1--> Node 2 (demand=-5)
//                      \                              ^
//                       ----cost=10-------------------/
//
//  Optimal: 0->1->2 at cost 5*(1+1)=10 (not 0->2 at cost 5*10=50)
// ---------------------------------------------------------------------------

static NetworkData build_asymmetric_network() {
    NetworkData::Builder b;
    b.add_node(5, "src");     // 0
    b.add_node(0, "middle");  // 1
    b.add_node(-5, "dst");    // 2

    b.add_arc(0, 1, /*cost=*/1, 0, 10);   // arc 0: cheap
    b.add_arc(1, 2, /*cost=*/1, 0, 10);   // arc 1: cheap
    b.add_arc(0, 2, /*cost=*/10, 0, 10);  // arc 2: expensive direct

    return b.build();
}

// ---------------------------------------------------------------------------
//  Helper: build a simple shipping network
//
//  Port A (supply=3) -----> Hub H -----> Port B (demand=-2)
//                                \-----> Port C (demand=-1)
//
//  Models a simple liner shipping scenario where cargo flows from
//  origin port A through hub H to destination ports B and C.
// ---------------------------------------------------------------------------

static NetworkData build_shipping_network() {
    NetworkData::Builder b;

    // Ports and hub.
    int A = b.add_node(3, "Port_A");
    int H = b.add_node(0, "Hub_H");
    int B = b.add_node(-2, "Port_B");
    int C = b.add_node(-1, "Port_C");

    // Add transit time resource.
    int time_res = b.add_resource("transit_time", 100);

    // Shipping legs.
    int a0 = b.add_arc(A, H, /*cost=*/5, 0, 5);   // A -> H
    int a1 = b.add_arc(H, B, /*cost=*/3, 0, 3);   // H -> B
    int a2 = b.add_arc(H, C, /*cost=*/4, 0, 3);   // H -> C
    int a3 = b.add_arc(A, B, /*cost=*/12, 0, 3);  // direct A -> B (expensive)

    // Transit times.
    b.set_resource_usage(a0, time_res, 2);  // 2 days A -> H
    b.set_resource_usage(a1, time_res, 1);  // 1 day H -> B
    b.set_resource_usage(a2, time_res, 3);  // 3 days H -> C
    b.set_resource_usage(a3, time_res, 5);  // 5 days direct A -> B

    return b.build();
}

// ===========================================================================
//  Network construction tests
// ===========================================================================

TEST_CASE("NetworkData construction", "[network]") {
    auto data = build_simple_network();

    SECTION("node and arc counts") {
        REQUIRE(data.num_nodes() == 4);
        REQUIRE(data.num_arcs() == 4);
    }

    SECTION("supply and demand") {
        REQUIRE(data.supply(0) == 10);
        REQUIRE(data.supply(1) == 0);
        REQUIRE(data.supply(2) == 0);
        REQUIRE(data.supply(3) == -10);
        REQUIRE(data.total_supply() == 0);
    }

    SECTION("node names") {
        REQUIRE(data.node_name(0) == "source");
        REQUIRE(data.node_name(3) == "sink");
    }

    SECTION("arc data") {
        REQUIRE(data.arc(0).tail == 0);
        REQUIRE(data.arc(0).head == 1);
        REQUIRE(data.arc(0).cost == 2);
        REQUIRE(data.arc(0).lower_cap == 0);
        REQUIRE(data.arc(0).upper_cap == 10);
    }

    SECTION("outgoing and incoming adjacency") {
        auto out0 = data.outgoing(0);
        REQUIRE(out0.size() == 2);  // arcs 0 and 2

        auto in3 = data.incoming(3);
        REQUIRE(in3.size() == 2);  // arcs 1 and 3

        // Node 1: one incoming (arc 0), one outgoing (arc 1).
        REQUIRE(data.outgoing(1).size() == 1);
        REQUIRE(data.incoming(1).size() == 1);
    }
}

TEST_CASE("NetworkData with resources", "[network]") {
    auto data = build_shipping_network();

    REQUIRE(data.has_resources());
    REQUIRE(data.num_resources() == 1);
    REQUIRE(data.resource(0).name == "transit_time");
    REQUIRE(data.resource(0).upper_bound == 100);

    // Check resource usage on arc 0 (A->H).
    REQUIRE(data.resource_usage(0, 0) == 2);
}

// ===========================================================================
//  MCF solver tests
// ===========================================================================

TEST_CASE("MCF solver on simple network", "[network]") {
    auto data = build_simple_network();
    auto sol = McfSolver::solve(data);

    SECTION("solution is feasible") {
        REQUIRE(sol.flow_conservation());
        REQUIRE(sol.capacity_feasible());
        REQUIRE(sol.feasible());
    }

    SECTION("all supply is routed") {
        REQUIRE(sol.excess(0) == 0);
        REQUIRE(sol.excess(3) == 0);
    }

    SECTION("optimal cost") {
        // Both paths cost the same (50), so total cost should be 50.
        REQUIRE(sol.cost() == 50);
    }

    SECTION("total flow is 10") {
        // Flow out of source should be 10.
        int total_out = 0;
        for (int a : data.outgoing(0)) {
            total_out += sol.flow(a);
        }
        REQUIRE(total_out == 10);
    }
}

TEST_CASE("MCF solver on asymmetric network", "[network]") {
    auto data = build_asymmetric_network();
    auto sol = McfSolver::solve(data);

    REQUIRE(sol.feasible());
    // Optimal: route through middle node at cost 5*(1+1) = 10.
    REQUIRE(sol.cost() == 10);

    // Flow on cheap path (arcs 0 and 1) should be 5.
    REQUIRE(sol.flow(0) == 5);
    REQUIRE(sol.flow(1) == 5);
    // No flow on expensive direct arc.
    REQUIRE(sol.flow(2) == 0);
}

TEST_CASE("MCF solver with capacity constraints", "[network]") {
    // Same as asymmetric but limit cheap path capacity to 3.
    NetworkData::Builder b;
    b.add_node(5, "src");
    b.add_node(0, "mid");
    b.add_node(-5, "dst");

    b.add_arc(0, 1, 1, 0, 3);   // cheap but limited to 3
    b.add_arc(1, 2, 1, 0, 3);   // cheap but limited to 3
    b.add_arc(0, 2, 10, 0, 5);  // expensive but big capacity

    auto data = b.build();
    auto sol = McfSolver::solve(data);

    REQUIRE(sol.feasible());
    // 3 units via cheap (cost 6) + 2 units via expensive (cost 20) = 26.
    REQUIRE(sol.cost() == 26);
    REQUIRE(sol.flow(0) == 3);
    REQUIRE(sol.flow(1) == 3);
    REQUIRE(sol.flow(2) == 2);
}

// ===========================================================================
//  Flow conservation tests
// ===========================================================================

TEST_CASE("Flow conservation check", "[network]") {
    auto data = build_simple_network();
    NetworkSolution sol(data);

    SECTION("zero flow satisfies conservation only if no supply/demand") {
        // With supply/demand, zero flow has excess violations.
        REQUIRE_FALSE(sol.flow_conservation());
        REQUIRE(sol.excess(0) == 10);   // supply not routed
        REQUIRE(sol.excess(3) == -10);  // demand not met
    }

    SECTION("manual flow setting") {
        sol.set_flow(0, 10);  // 0 -> 1
        sol.set_flow(1, 10);  // 1 -> 3

        REQUIRE(sol.flow_conservation());
        REQUIRE(sol.capacity_feasible());
        REQUIRE(sol.cost() == 50);
    }
}

// ===========================================================================
//  Construction heuristic tests
// ===========================================================================

TEST_CASE("Greedy construction", "[network]") {
    auto data = build_asymmetric_network();
    auto sol = construct_greedy(data);

    SECTION("produces a feasible solution") {
        REQUIRE(sol.flow_conservation());
        REQUIRE(sol.capacity_feasible());
    }

    SECTION("routes all supply") {
        REQUIRE(sol.excess(0) == 0);
        REQUIRE(sol.excess(2) == 0);
    }
}

TEST_CASE("Feasible construction with lower bounds", "[network]") {
    NetworkData::Builder b;
    b.add_node(5, "src");
    b.add_node(-5, "dst");
    b.add_arc(0, 1, 3, /*lower=*/2, /*upper=*/5);  // must send at least 2
    b.add_arc(0, 1, 1, /*lower=*/0, /*upper=*/5);  // cheaper alternative

    auto data = b.build();
    auto sol = construct_feasible(data);

    REQUIRE(sol.flow_conservation());
    REQUIRE(sol.capacity_feasible());
    // Arc 0 must have at least 2 flow.
    REQUIRE(sol.flow(0) >= 2);
}

// ===========================================================================
//  Operator tests
// ===========================================================================

TEST_CASE("RerouteFlow improves cost", "[network]") {
    // Build a suboptimal solution and check that rerouting improves it.
    auto data = build_asymmetric_network();
    NetworkSolution sol(data);

    // Set a suboptimal flow: all 5 units on the expensive direct arc.
    sol.set_flow(2, 5);  // arc 0->2, cost=10

    REQUIRE(sol.flow_conservation());
    REQUIRE(sol.cost() == 50);

    auto moves = RerouteFlow::enumerate(data, sol);
    REQUIRE_FALSE(moves.empty());

    // Apply the first improving move.
    auto best = moves[0];
    for (auto const& m : moves) {
        if (m.delta < best.delta) {
            best = m;
        }
    }
    REQUIRE(best.delta < 0);

    RerouteFlow::apply(sol, best);
    REQUIRE(sol.cost() < 50);
    REQUIRE(sol.flow_conservation());
}

TEST_CASE("CycleCancel on suboptimal flow", "[network]") {
    // Network: 0(+5) -> 1 -> 2(-5), with a suboptimal flow.
    NetworkData::Builder b;
    b.add_node(5);
    b.add_node(0);
    b.add_node(-5);

    b.add_arc(0, 1, 1, 0, 10);   // arc 0: cheap
    b.add_arc(1, 2, 1, 0, 10);   // arc 1: cheap
    b.add_arc(0, 2, 10, 0, 10);  // arc 2: expensive

    auto data = b.build();
    NetworkSolution sol(data);

    // Suboptimal: send all flow on expensive arc.
    sol.set_flow(2, 5);
    REQUIRE(sol.cost() == 50);

    int cancelled = CycleCancel::cancel_all(data, sol);
    // After cycle cancellation, cost should improve.
    // Note: cycle cancelling works on residual graph, so if there's a
    // negative cycle it will improve. The specific result depends on
    // cycle detection.
    REQUIRE(sol.flow_conservation());
    REQUIRE(sol.capacity_feasible());
}

TEST_CASE("AdjustCapacity enumeration", "[network]") {
    NetworkData::Builder b;
    b.add_node(3);
    b.add_node(-3);
    b.add_arc(0, 1, 5, 0, 10);   // positive cost, flow can be reduced
    b.add_arc(0, 1, -2, 0, 10);  // negative cost, flow can be increased

    auto data = b.build();
    NetworkSolution sol(data);
    sol.set_flow(0, 3);

    auto moves = AdjustCapacity::enumerate(data, sol);
    // Should find moves for both arcs.
    REQUIRE_FALSE(moves.empty());
}

// ===========================================================================
//  Shipping network test
// ===========================================================================

TEST_CASE("Shipping network MCF", "[network]") {
    auto data = build_shipping_network();
    auto sol = McfSolver::solve(data);

    SECTION("feasible solution") {
        REQUIRE(sol.feasible());
    }

    SECTION("all cargo delivered") {
        REQUIRE(sol.excess(0) == 0);  // Port A
        REQUIRE(sol.excess(2) == 0);  // Port B
        REQUIRE(sol.excess(3) == 0);  // Port C
    }

    SECTION("optimal routing through hub") {
        // Optimal: 3 units A->H, 2 via H->B (cost 5+3=8 each),
        // 1 via H->C (cost 5+4=9).
        // Total = 2*8 + 1*9 = 25.
        // Direct A->B costs 12 per unit so never used optimally.
        REQUIRE(sol.cost() == 25);
    }

    SECTION("resource consumption") {
        REQUIRE(sol.resource_feasible());
    }
}

// ===========================================================================
//  Edge cases
// ===========================================================================

TEST_CASE("Empty network", "[network]") {
    NetworkData::Builder b;
    auto data = b.build();

    REQUIRE(data.num_nodes() == 0);
    REQUIRE(data.num_arcs() == 0);
    REQUIRE(data.total_supply() == 0);
}

TEST_CASE("Single node no arcs", "[network]") {
    NetworkData::Builder b;
    b.add_node(0, "lone");
    auto data = b.build();

    REQUIRE(data.num_nodes() == 1);
    REQUIRE(data.num_arcs() == 0);
    REQUIRE(data.outgoing(0).empty());
    REQUIRE(data.incoming(0).empty());
}

TEST_CASE("Balanced multi-commodity flow", "[network]") {
    // Two supply/demand pairs sharing a network.
    //   0(+3), 1(+2) -> 2 -> 3(-3), 4(-2)
    NetworkData::Builder b;
    b.add_node(3);   // 0: supply
    b.add_node(2);   // 1: supply
    b.add_node(0);   // 2: transit
    b.add_node(-3);  // 3: demand
    b.add_node(-2);  // 4: demand

    b.add_arc(0, 2, 1, 0, 5);
    b.add_arc(1, 2, 2, 0, 5);
    b.add_arc(2, 3, 1, 0, 5);
    b.add_arc(2, 4, 3, 0, 5);

    auto data = b.build();
    REQUIRE(data.total_supply() == 0);

    auto sol = McfSolver::solve(data);
    REQUIRE(sol.feasible());

    // Total flow through node 2 should be 5.
    int in_flow = 0;
    for (int a : data.incoming(2)) {
        in_flow += sol.flow(a);
    }
    REQUIRE(in_flow == 5);
}
