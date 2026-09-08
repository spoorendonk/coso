#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <routing/problem_data.h>
#include <set>
#include <vector>

using namespace coso;

// =========================================================================
//  Helper: build a simple CVRP instance (1 depot, N clients in a line)
// =========================================================================

static ProblemData make_line_instance(int num_clients, int granular_k = 0) {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    for (int i = 1; i <= num_clients; ++i) {
        b.add_client({static_cast<double>(i * 10), 0.0},
                     {.demand = {1}, .tw = {0, 1000}, .service = 5});
    }
    b.add_vehicle_type(2, {.capacity = {100}});
    return b.build(granular_k);
}

// =========================================================================
//  Construction and basic accessors
// =========================================================================

TEST_CASE("ProblemData counts depots, clients, vehicle types", "[problem_data]") {
    auto pd = make_line_instance(5);
    REQUIRE(pd.num_depots() == 1);
    REQUIRE(pd.num_clients() == 5);
    REQUIRE(pd.num_vehicle_types() == 1);
    REQUIRE(pd.num_nodes() == 6);
    REQUIRE(pd.num_profiles() == 1);
}

TEST_CASE("ProblemData depot data is accessible", "[problem_data]") {
    ProblemData::Builder b;
    b.add_depot({100.0, 200.0}, {.tw = {0, 500}});
    auto pd = b.build(0);

    REQUIRE(pd.num_depots() == 1);
    REQUIRE(pd.depot(0).coord.x == 100.0);
    REQUIRE(pd.depot(0).coord.y == 200.0);
    REQUIRE(pd.depot(0).tw.start == 0);
    REQUIRE(pd.depot(0).tw.end == 500);
}

TEST_CASE("ProblemData client data preserves attributes", "[problem_data]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_client({10.0, 20.0}, {
                                   .demand = {5, 3},
                                   .tw = {100, 200},
                                   .service = 15,
                                   .prize = 10,
                                   .required = false,
                               });
    b.add_vehicle_type(1, {.capacity = {50, 30}});
    auto pd = b.build(0);

    REQUIRE(pd.num_clients() == 1);
    auto const& c = pd.client(0);
    REQUIRE(c.coord.x == 10.0);
    REQUIRE(c.coord.y == 20.0);
    REQUIRE(c.demand.size() == 2);
    REQUIRE(c.demand[0] == 5);
    REQUIRE(c.demand[1] == 3);
    REQUIRE(c.tw.start == 100);
    REQUIRE(c.tw.end == 200);
    REQUIRE(c.service == 15);
    REQUIRE(c.prize == 10);
    REQUIRE_FALSE(c.required);
}

TEST_CASE("ProblemData vehicle type data preserves attributes", "[problem_data]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {
                              .capacity = {100, 50},
                              .max_duration = 500,
                              .max_distance = 300,
                          });
    auto pd = b.build(0);

    REQUIRE(pd.num_vehicle_types() == 1);
    auto const& vt = pd.vehicle_type(0);
    REQUIRE(vt.count == 3);
    REQUIRE(vt.capacity[0] == 100);
    REQUIRE(vt.capacity[1] == 50);
    REQUIRE(vt.max_duration == 500);
    REQUIRE(vt.max_distance == 300);
}

TEST_CASE("ProblemData total_vehicles sums across types", "[problem_data]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(3, {.capacity = {100}});
    b.add_vehicle_type(2, {.capacity = {200}});
    auto pd = b.build(0);

    REQUIRE(pd.total_vehicles() == 5);
}

TEST_CASE("ProblemData num_load_dims is max of demand/capacity dims", "[problem_data]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_client({10.0, 0.0}, {.demand = {5, 3, 1}});
    b.add_client({20.0, 0.0}, {.demand = {2}});
    b.add_vehicle_type(1, {.capacity = {100, 50}});
    auto pd = b.build(0);

    REQUIRE(pd.num_load_dims() == 3);
    // Client with fewer dims should be zero-padded.
    REQUIRE(pd.client(1).demand.size() == 3);
    REQUIRE(pd.client(1).demand[1] == 0);
    REQUIRE(pd.client(1).demand[2] == 0);
    // Vehicle capacity should also be zero-padded.
    REQUIRE(pd.vehicle_type(0).capacity.size() == 3);
    REQUIRE(pd.vehicle_type(0).capacity[2] == 0);
}

// =========================================================================
//  Distance matrix (Euclidean)
// =========================================================================

TEST_CASE("Euclidean distances are computed correctly", "[problem_data]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_client({3.0, 4.0});  // distance from depot = 5
    b.add_client({6.0, 8.0});  // distance from depot = 10
    auto pd = b.build(0);

    // Node 0 = depot, node 1 = client 0, node 2 = client 1.
    REQUIRE(pd.dist(0, 1) == 5);
    REQUIRE(pd.dist(0, 2) == 10);
    REQUIRE(pd.dist(1, 0) == 5);
    REQUIRE(pd.dist(2, 0) == 10);

    // Client-to-client: sqrt((6-3)^2 + (8-4)^2) = sqrt(25) = 5
    REQUIRE(pd.dist(1, 2) == 5);
    REQUIRE(pd.dist(2, 1) == 5);

    // Self-distance = 0.
    REQUIRE(pd.dist(0, 0) == 0);
    REQUIRE(pd.dist(1, 1) == 0);
}

TEST_CASE("Duration defaults to distance", "[problem_data]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_client({3.0, 4.0});
    auto pd = b.build(0);

    REQUIRE(pd.dur(0, 1) == pd.dist(0, 1));
}

// =========================================================================
//  Explicit distance overrides
// =========================================================================

TEST_CASE("Explicit distances override Euclidean", "[problem_data]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_client({3.0, 4.0});
    b.set_distance(0, 0, 1, 42);
    auto pd = b.build(0);

    REQUIRE(pd.dist(0, 1) == 42);
    // Reverse not explicitly set: still Euclidean.
    REQUIRE(pd.dist(1, 0) == 5);
}

TEST_CASE("Explicit durations override default", "[problem_data]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_client({3.0, 4.0});
    b.set_duration(0, 0, 1, 99);
    auto pd = b.build(0);

    REQUIRE(pd.dur(0, 1) == 99);
    // Reverse not set: still defaults to Euclidean.
    REQUIRE(pd.dur(1, 0) == 5);
}

// =========================================================================
//  Pickup-delivery requests
// =========================================================================

TEST_CASE("Pickup-delivery requests are stored", "[problem_data]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_client({10.0, 0.0});  // client 0 = pickup
    b.add_client({20.0, 0.0});  // client 1 = delivery
    b.add_request(0, 1);
    auto pd = b.build(0);

    REQUIRE(pd.requests().size() == 1);
    REQUIRE(pd.requests()[0].pickup == 0);
    REQUIRE(pd.requests()[0].delivery == 1);
}

// =========================================================================
//  Granular neighbour lists
// =========================================================================

TEST_CASE("Granular neighbours with k=3", "[problem_data]") {
    // Depot at origin, 5 clients along x-axis at 10, 20, 30, 40, 50.
    auto pd = make_line_instance(5, 3);

    REQUIRE(pd.granular_k() == 3);

    // Client 0 (node 1, at x=10): nearest nodes are depot(0), client1(2), client2(3).
    auto nb0 = pd.neighbours(0);
    REQUIRE(nb0.size() == 3);
    // The depot at distance 10 and next client at distance 10 should be closest.
    std::set<int> nb0_set(nb0.begin(), nb0.end());
    REQUIRE(nb0_set.contains(0));  // depot at distance 10
    REQUIRE(nb0_set.contains(2));  // client 1 at distance 10
    REQUIRE(nb0_set.contains(3));  // client 2 at distance 20
}

TEST_CASE("Granular neighbours sorted by distance", "[problem_data]") {
    auto pd = make_line_instance(5, 3);

    // For each client, verify neighbours are sorted by distance.
    for (int c = 0; c < pd.num_clients(); ++c) {
        auto nb = pd.neighbours(c);
        int c_node = pd.num_depots() + c;
        for (int i = 1; i < static_cast<int>(nb.size()); ++i) {
            if (nb[i] == -1) {
                break;
            }
            REQUIRE(pd.dist(c_node, nb[i - 1]) <= pd.dist(c_node, nb[i]));
        }
    }
}

TEST_CASE("Granular k clamped to num_clients-1", "[problem_data]") {
    // Only 2 clients, but request k=40.
    auto pd = make_line_instance(2, 40);

    // k should be clamped to min(40, 2-1) = 1... actually num_nodes-1 candidates
    // but granular_k is clamped to num_clients-1 = 1.
    REQUIRE(pd.granular_k() == 1);

    // Client 0 should have 1 neighbour.
    auto nb = pd.neighbours(0);
    REQUIRE(nb.size() == 1);
}

TEST_CASE("Granular k=0 means no neighbours", "[problem_data]") {
    auto pd = make_line_instance(5, 0);
    REQUIRE(pd.granular_k() == 0);
}

// =========================================================================
//  node_coord
// =========================================================================

TEST_CASE("node_coord returns correct coordinates", "[problem_data]") {
    ProblemData::Builder b;
    b.add_depot({100.0, 200.0});
    b.add_client({10.0, 20.0});
    b.add_client({30.0, 40.0});
    auto pd = b.build(0);

    auto d = pd.node_coord(0);
    REQUIRE(d.x == 100.0);
    REQUIRE(d.y == 200.0);

    auto c0 = pd.node_coord(1);
    REQUIRE(c0.x == 10.0);
    REQUIRE(c0.y == 20.0);

    auto c1 = pd.node_coord(2);
    REQUIRE(c1.x == 30.0);
    REQUIRE(c1.y == 40.0);
}

// =========================================================================
//  Edge cases
// =========================================================================

TEST_CASE("Empty ProblemData (no clients)", "[problem_data]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(1, {.capacity = {10}});
    auto pd = b.build(0);

    REQUIRE(pd.num_clients() == 0);
    REQUIRE(pd.num_depots() == 1);
    REQUIRE(pd.num_nodes() == 1);
    REQUIRE(pd.total_vehicles() == 1);
    REQUIRE(pd.dist(0, 0) == 0);
}

TEST_CASE("Multiple depots", "[problem_data]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_depot({100.0, 0.0});
    b.add_client({50.0, 0.0});
    auto pd = b.build(0);

    REQUIRE(pd.num_depots() == 2);
    REQUIRE(pd.num_nodes() == 3);
    // Depot 0 to client: 50
    REQUIRE(pd.dist(0, 2) == 50);
    // Depot 1 to client: 50
    REQUIRE(pd.dist(1, 2) == 50);
    // Depot 0 to depot 1: 100
    REQUIRE(pd.dist(0, 1) == 100);
}

TEST_CASE("Asymmetric explicit distances", "[problem_data]") {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_client({0.0, 0.0});  // same location (Euclidean = 0)
    b.set_distance(0, 0, 1, 10);
    b.set_distance(0, 1, 0, 20);
    auto pd = b.build(0);

    REQUIRE(pd.dist(0, 1) == 10);
    REQUIRE(pd.dist(1, 0) == 20);
}
