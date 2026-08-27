#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <model/instance_reader.h>
#include <string>

// =========================================================================
//  CVRPLIB instance reader tests
// =========================================================================

// A small 5-node CVRP instance in standard .vrp format (1 depot + 4 clients).
static const std::string SMALL_VRP = R"(
NAME : test-n5-k2
COMMENT : Small test instance
TYPE : CVRP
DIMENSION : 5
CAPACITY : 100
EDGE_WEIGHT_TYPE : EUC_2D
NODE_COORD_SECTION
1 0 0
2 10 0
3 0 10
4 10 10
5 5 5
DEMAND_SECTION
1 0
2 30
3 40
4 20
5 10
DEPOT_SECTION
1
-1
EOF
)";

TEST_CASE("parse_vrp reads metadata", "[instance_reader]") {
    auto inst = coso::parse_vrp(SMALL_VRP);
    REQUIRE(inst.name == "test-n5-k2");
    REQUIRE(inst.comment == "Small test instance");
    REQUIRE(inst.type == "CVRP");
    REQUIRE(inst.dimension == 5);
    REQUIRE(inst.capacity == 100);
    REQUIRE(inst.edge_weight_type == coso::EdgeWeightType::EUC_2D);
}

TEST_CASE("parse_vrp reads coordinates", "[instance_reader]") {
    auto inst = coso::parse_vrp(SMALL_VRP);
    REQUIRE(inst.coords.size() == 5);
    // Node 1 (index 0) is at (0, 0).
    REQUIRE(inst.coords[0].x == 0.0);
    REQUIRE(inst.coords[0].y == 0.0);
    // Node 2 (index 1) is at (10, 0).
    REQUIRE(inst.coords[1].x == 10.0);
    REQUIRE(inst.coords[1].y == 0.0);
    // Node 5 (index 4) is at (5, 5).
    REQUIRE(inst.coords[4].x == 5.0);
    REQUIRE(inst.coords[4].y == 5.0);
}

TEST_CASE("parse_vrp reads demands", "[instance_reader]") {
    auto inst = coso::parse_vrp(SMALL_VRP);
    REQUIRE(inst.demands.size() == 5);
    REQUIRE(inst.demands[0] == 0);  // depot
    REQUIRE(inst.demands[1] == 30);
    REQUIRE(inst.demands[2] == 40);
    REQUIRE(inst.demands[3] == 20);
    REQUIRE(inst.demands[4] == 10);
}

TEST_CASE("parse_vrp reads depot section", "[instance_reader]") {
    auto inst = coso::parse_vrp(SMALL_VRP);
    REQUIRE(inst.depot_ids.size() == 1);
    REQUIRE(inst.depot_ids[0] == 0);  // 1-based id "1" -> 0-based index 0
}

TEST_CASE("EUC_2D distance computation", "[instance_reader]") {
    auto inst = coso::parse_vrp(SMALL_VRP);
    // Distance from (0,0) to (10,0) = 10.
    REQUIRE(inst.dist(0, 1) == 10);
    // Distance from (0,0) to (0,10) = 10.
    REQUIRE(inst.dist(0, 2) == 10);
    // Distance from (0,0) to (10,10) = sqrt(200) ~ 14.14 -> round to 14.
    REQUIRE(inst.dist(0, 3) == 14);
    // Distance from (0,0) to (5,5) = sqrt(50) ~ 7.07 -> round to 7.
    REQUIRE(inst.dist(0, 4) == 7);
    // Self-distance = 0.
    REQUIRE(inst.dist(0, 0) == 0);
    // Symmetry.
    REQUIRE(inst.dist(1, 0) == inst.dist(0, 1));
}

// Instance with CEIL_2D edge weight type.
static const std::string CEIL_VRP = R"(
NAME : ceil-test
TYPE : CVRP
DIMENSION : 3
CAPACITY : 50
EDGE_WEIGHT_TYPE : CEIL_2D
NODE_COORD_SECTION
1 0 0
2 3 4
3 6 8
DEMAND_SECTION
1 0
2 10
3 20
DEPOT_SECTION
1
-1
EOF
)";

TEST_CASE("CEIL_2D uses ceiling for distances", "[instance_reader]") {
    auto inst = coso::parse_vrp(CEIL_VRP);
    // Distance from (0,0) to (3,4) = 5.0 (exact).
    REQUIRE(inst.dist(0, 1) == 5);
    // Distance from (0,0) to (6,8) = 10.0 (exact).
    REQUIRE(inst.dist(0, 2) == 10);
}

// Instance with explicit FULL_MATRIX.
static const std::string EXPLICIT_FULL_VRP = R"(
NAME : explicit-full
TYPE : CVRP
DIMENSION : 3
CAPACITY : 50
EDGE_WEIGHT_TYPE : EXPLICIT
EDGE_WEIGHT_FORMAT : FULL_MATRIX
EDGE_WEIGHT_SECTION
0 10 20
10 0 15
20 15 0
DEMAND_SECTION
1 0
2 10
3 20
DEPOT_SECTION
1
-1
EOF
)";

TEST_CASE("EXPLICIT FULL_MATRIX parsing", "[instance_reader]") {
    auto inst = coso::parse_vrp(EXPLICIT_FULL_VRP);
    REQUIRE(inst.edge_weight_type == coso::EdgeWeightType::EXPLICIT);
    REQUIRE(inst.distance_matrix.size() == 9);
    REQUIRE(inst.dist(0, 0) == 0);
    REQUIRE(inst.dist(0, 1) == 10);
    REQUIRE(inst.dist(0, 2) == 20);
    REQUIRE(inst.dist(1, 2) == 15);
    REQUIRE(inst.dist(2, 1) == 15);
}

// Instance with LOWER_DIAG_ROW format.
static const std::string EXPLICIT_LOWER_DIAG_VRP = R"(
NAME : explicit-lower-diag
TYPE : CVRP
DIMENSION : 4
CAPACITY : 100
EDGE_WEIGHT_TYPE : EXPLICIT
EDGE_WEIGHT_FORMAT : LOWER_DIAG_ROW
EDGE_WEIGHT_SECTION
0
5 0
10 8 0
15 12 7 0
DEMAND_SECTION
1 0
2 10
3 20
4 30
DEPOT_SECTION
1
-1
EOF
)";

TEST_CASE("EXPLICIT LOWER_DIAG_ROW parsing", "[instance_reader]") {
    auto inst = coso::parse_vrp(EXPLICIT_LOWER_DIAG_VRP);
    REQUIRE(inst.dimension == 4);
    REQUIRE(inst.distance_matrix.size() == 16);
    // Diagonal should be zero.
    REQUIRE(inst.dist(0, 0) == 0);
    REQUIRE(inst.dist(1, 1) == 0);
    // Symmetric off-diagonal entries.
    REQUIRE(inst.dist(1, 0) == 5);
    REQUIRE(inst.dist(0, 1) == 5);
    REQUIRE(inst.dist(2, 0) == 10);
    REQUIRE(inst.dist(2, 1) == 8);
    REQUIRE(inst.dist(3, 0) == 15);
    REQUIRE(inst.dist(3, 1) == 12);
    REQUIRE(inst.dist(3, 2) == 7);
}

// Instance with UPPER_ROW format (no diagonal).
static const std::string EXPLICIT_UPPER_ROW_VRP = R"(
NAME : explicit-upper-row
TYPE : CVRP
DIMENSION : 3
CAPACITY : 50
EDGE_WEIGHT_TYPE : EXPLICIT
EDGE_WEIGHT_FORMAT : UPPER_ROW
EDGE_WEIGHT_SECTION
10 20
15
DEMAND_SECTION
1 0
2 10
3 20
DEPOT_SECTION
1
-1
EOF
)";

TEST_CASE("EXPLICIT UPPER_ROW parsing", "[instance_reader]") {
    auto inst = coso::parse_vrp(EXPLICIT_UPPER_ROW_VRP);
    REQUIRE(inst.dist(0, 1) == 10);
    REQUIRE(inst.dist(0, 2) == 20);
    REQUIRE(inst.dist(1, 2) == 15);
    // Symmetric.
    REQUIRE(inst.dist(1, 0) == 10);
    REQUIRE(inst.dist(2, 0) == 20);
    // Diagonal is zero (not stored).
    REQUIRE(inst.dist(0, 0) == 0);
}

// Instance with LOWER_ROW format (no diagonal).
static const std::string EXPLICIT_LOWER_ROW_VRP = R"(
NAME : explicit-lower-row
TYPE : CVRP
DIMENSION : 3
CAPACITY : 50
EDGE_WEIGHT_TYPE : EXPLICIT
EDGE_WEIGHT_FORMAT : LOWER_ROW
EDGE_WEIGHT_SECTION
10
20 15
DEMAND_SECTION
1 0
2 10
3 20
DEPOT_SECTION
1
-1
EOF
)";

TEST_CASE("EXPLICIT LOWER_ROW parsing", "[instance_reader]") {
    auto inst = coso::parse_vrp(EXPLICIT_LOWER_ROW_VRP);
    REQUIRE(inst.dist(1, 0) == 10);
    REQUIRE(inst.dist(2, 0) == 20);
    REQUIRE(inst.dist(2, 1) == 15);
    // Symmetric.
    REQUIRE(inst.dist(0, 1) == 10);
    REQUIRE(inst.dist(0, 0) == 0);
}

// Test that missing DIMENSION throws.
TEST_CASE("parse_vrp throws on missing dimension", "[instance_reader]") {
    const std::string bad = R"(
NAME : bad
TYPE : CVRP
CAPACITY : 100
EDGE_WEIGHT_TYPE : EUC_2D
NODE_COORD_SECTION
1 0 0
DEMAND_SECTION
1 0
DEPOT_SECTION
1
-1
EOF
)";
    REQUIRE_THROWS_AS(coso::parse_vrp(bad), std::runtime_error);
}

// Test that EXPLICIT without EDGE_WEIGHT_SECTION throws.
TEST_CASE("parse_vrp throws on missing edge weight section", "[instance_reader]") {
    const std::string bad = R"(
NAME : bad
TYPE : CVRP
DIMENSION : 3
CAPACITY : 50
EDGE_WEIGHT_TYPE : EXPLICIT
EDGE_WEIGHT_FORMAT : FULL_MATRIX
DEMAND_SECTION
1 0
2 10
3 20
DEPOT_SECTION
1
-1
EOF
)";
    REQUIRE_THROWS_AS(coso::parse_vrp(bad), std::runtime_error);
}

// Test that read_vrp throws on non-existent file.
TEST_CASE("read_vrp throws on missing file", "[instance_reader]") {
    REQUIRE_THROWS_AS(coso::read_vrp("nonexistent.vrp"), std::runtime_error);
}

// Test multiple depots.
static const std::string MULTI_DEPOT_VRP = R"(
NAME : multi-depot
TYPE : CVRP
DIMENSION : 4
CAPACITY : 100
EDGE_WEIGHT_TYPE : EUC_2D
NODE_COORD_SECTION
1 0 0
2 10 0
3 5 5
4 5 10
DEMAND_SECTION
1 0
2 0
3 30
4 40
DEPOT_SECTION
1
2
-1
EOF
)";

TEST_CASE("parse_vrp supports multiple depots", "[instance_reader]") {
    auto inst = coso::parse_vrp(MULTI_DEPOT_VRP);
    REQUIRE(inst.depot_ids.size() == 2);
    REQUIRE(inst.depot_ids[0] == 0);
    REQUIRE(inst.depot_ids[1] == 1);
}

// Test VEHICLES and DISTANCE metadata fields.
static const std::string META_VRP = R"(
NAME : meta-test
TYPE : CVRP
DIMENSION : 3
CAPACITY : 50
VEHICLES : 5
DISTANCE : 123.45
EDGE_WEIGHT_TYPE : EUC_2D
NODE_COORD_SECTION
1 0 0
2 10 0
3 0 10
DEMAND_SECTION
1 0
2 10
3 20
DEPOT_SECTION
1
-1
EOF
)";

TEST_CASE("parse_vrp reads optional metadata", "[instance_reader]") {
    auto inst = coso::parse_vrp(META_VRP);
    REQUIRE(inst.vehicles == 5);
    REQUIRE(inst.distance == 123.45);
}
