// The published values a Solomon route set is priced against, per convention (#249).
//
// - floor(10 * euclid), times scaled by 10: Solomon's own table of exact-method optima, C101 =
// 827.3
//   (KDMSS, web.cba.neu.edu/~msolomon/c1c2solu.htm). Holds in every build.
// - Double precision, totals rounded to two decimals: SINTEF's best-known table, C101 = 828.94
//   (sintef.no/projectweb/top/vrptw/100-customers/). Needs COSO_MEASURE=double.
//
// The route set is the one SINTEF links for C101's best-known row (Hasle & Kloster, c101.txt). C101
// is embedded so that the gate does not depend on a download.
#include "model/routing_model.h"
#include "model/types.h"
#include "routing/problem_data.h"
#include "routing/route.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <type_traits>
#include <unistd.h>
#include <vector>

using namespace coso;

namespace {

struct Row {
    int x, y, demand, ready, due, service;
};

// C101, as distributed: row 0 is the depot, row i is client i.
constexpr Row kC101[] = {
    {40, 50, 0, 0, 1236, 0},     {45, 68, 10, 912, 967, 90},   {45, 70, 30, 825, 870, 90},
    {42, 66, 10, 65, 146, 90},   {42, 68, 10, 727, 782, 90},   {42, 65, 10, 15, 67, 90},
    {40, 69, 20, 621, 702, 90},  {40, 66, 20, 170, 225, 90},   {38, 68, 20, 255, 324, 90},
    {38, 70, 10, 534, 605, 90},  {35, 66, 10, 357, 410, 90},   {35, 69, 10, 448, 505, 90},
    {25, 85, 20, 652, 721, 90},  {22, 75, 30, 30, 92, 90},     {22, 85, 10, 567, 620, 90},
    {20, 80, 40, 384, 429, 90},  {20, 85, 40, 475, 528, 90},   {18, 75, 20, 99, 148, 90},
    {15, 75, 20, 179, 254, 90},  {15, 80, 10, 278, 345, 90},   {30, 50, 10, 10, 73, 90},
    {30, 52, 20, 914, 965, 90},  {28, 52, 20, 812, 883, 90},   {28, 55, 10, 732, 777, 90},
    {25, 50, 10, 65, 144, 90},   {25, 52, 40, 169, 224, 90},   {25, 55, 10, 622, 701, 90},
    {23, 52, 10, 261, 316, 90},  {23, 55, 20, 546, 593, 90},   {20, 50, 10, 358, 405, 90},
    {20, 55, 10, 449, 504, 90},  {10, 35, 20, 200, 237, 90},   {10, 40, 30, 31, 100, 90},
    {8, 40, 40, 87, 158, 90},    {8, 45, 20, 751, 816, 90},    {5, 35, 10, 283, 344, 90},
    {5, 45, 10, 665, 716, 90},   {2, 40, 20, 383, 434, 90},    {0, 40, 30, 479, 522, 90},
    {0, 45, 20, 567, 624, 90},   {35, 30, 10, 264, 321, 90},   {35, 32, 10, 166, 235, 90},
    {33, 32, 20, 68, 149, 90},   {33, 35, 10, 16, 80, 90},     {32, 30, 10, 359, 412, 90},
    {30, 30, 10, 541, 600, 90},  {30, 32, 30, 448, 509, 90},   {30, 35, 10, 1054, 1127, 90},
    {28, 30, 10, 632, 693, 90},  {28, 35, 10, 1001, 1066, 90}, {26, 32, 10, 815, 880, 90},
    {25, 30, 10, 725, 786, 90},  {25, 35, 10, 912, 969, 90},   {44, 5, 20, 286, 347, 90},
    {42, 10, 40, 186, 257, 90},  {42, 15, 10, 95, 158, 90},    {40, 5, 30, 385, 436, 90},
    {40, 15, 40, 35, 87, 90},    {38, 5, 30, 471, 534, 90},    {38, 15, 10, 651, 740, 90},
    {35, 5, 20, 562, 629, 90},   {50, 30, 10, 531, 610, 90},   {50, 35, 20, 262, 317, 90},
    {50, 40, 50, 171, 218, 90},  {48, 30, 10, 632, 693, 90},   {48, 40, 10, 76, 129, 90},
    {47, 35, 10, 826, 875, 90},  {47, 40, 10, 12, 77, 90},     {45, 30, 10, 734, 777, 90},
    {45, 35, 10, 916, 969, 90},  {95, 30, 30, 387, 456, 90},   {95, 35, 20, 293, 360, 90},
    {53, 30, 10, 450, 505, 90},  {92, 30, 10, 478, 551, 90},   {53, 35, 50, 353, 412, 90},
    {45, 65, 20, 997, 1068, 90}, {90, 35, 10, 203, 260, 90},   {88, 30, 10, 574, 643, 90},
    {88, 35, 20, 109, 170, 90},  {87, 30, 10, 668, 731, 90},   {85, 25, 10, 769, 820, 90},
    {85, 35, 30, 47, 124, 90},   {75, 55, 20, 369, 420, 90},   {72, 55, 10, 265, 338, 90},
    {70, 58, 20, 458, 523, 90},  {68, 60, 30, 555, 612, 90},   {66, 55, 10, 173, 238, 90},
    {65, 55, 20, 85, 144, 90},   {65, 60, 30, 645, 708, 90},   {63, 58, 10, 737, 802, 90},
    {60, 55, 10, 20, 84, 90},    {60, 60, 10, 836, 889, 90},   {67, 85, 20, 368, 441, 90},
    {65, 85, 40, 475, 518, 90},  {65, 82, 10, 285, 336, 90},   {62, 80, 30, 196, 239, 90},
    {60, 80, 10, 95, 156, 90},   {60, 85, 30, 561, 622, 90},   {58, 75, 20, 30, 84, 90},
    {55, 80, 10, 743, 820, 90},  {55, 85, 20, 647, 726, 90},
};
constexpr int kNumNodes = static_cast<int>(std::size(kC101));
constexpr int kCapacity = 200;
constexpr int kVehicles = 25;

// Published client ids, 1-based as in the instance file.
std::vector<std::vector<int>> const kC101Routes = {
    {81, 78, 76, 71, 70, 73, 77, 79, 80},
    {57, 55, 54, 53, 56, 58, 60, 59},
    {98, 96, 95, 94, 92, 93, 97, 100, 99},
    {32, 33, 31, 35, 37, 38, 39, 36, 34},
    {13, 17, 18, 19, 15, 16, 14, 12},
    {90, 87, 86, 83, 82, 84, 85, 88, 89, 91},
    {43, 42, 41, 40, 44, 46, 45, 48, 51, 50, 52, 49, 47},
    {67, 65, 63, 62, 74, 72, 61, 64, 68, 66, 69},
    {5, 3, 7, 8, 10, 11, 9, 6, 4, 2, 1, 75},
    {20, 24, 25, 27, 29, 30, 28, 26, 23, 22, 21},
};

double euclid(Row const& a, Row const& b) {
    return std::hypot(static_cast<double>(a.x - b.x), static_cast<double>(a.y - b.y));
}

/// C101 with times multiplied by `scale`, and arcs from `arc` when it is given.
template <typename Arc>
ProblemData build_c101(int scale, Arc arc) {
    ProblemData::Builder b;
    auto window = [&](Row const& r) {
        return TimeWindow{static_cast<Measure>(r.ready * scale),
                          static_cast<Measure>(r.due * scale)};
    };
    DepotParams depot;
    depot.tw = window(kC101[0]);
    b.add_depot({static_cast<double>(kC101[0].x), static_cast<double>(kC101[0].y)}, depot);
    for (int i = 1; i < kNumNodes; ++i) {
        Row const& r = kC101[i];
        ClientParams c;
        c.demand = {r.demand};
        c.tw = window(r);
        c.service = static_cast<Measure>(r.service * scale);
        b.add_client({static_cast<double>(r.x), static_cast<double>(r.y)}, c);
    }
    VehicleTypeParams v;
    v.capacity = {kCapacity};
    b.add_vehicle_type(kVehicles, v);
    for (int i = 0; i < kNumNodes; ++i) {
        for (int j = 0; j < kNumNodes; ++j) {
            Measure d = arc(kC101[i], kC101[j]);
            b.set_distance(0, i, j, d);
            b.set_duration(0, i, j, d);
        }
    }
    return b.build(0);
}

/// Total distance of the published routes, requiring each to be feasible.
Measure reprice(ProblemData const& data) {
    Measure total = 0;
    int served = 0;
    for (auto const& ids : kC101Routes) {
        std::vector<int> clients;
        for (int id : ids) {
            clients.push_back(id - 1);
        }
        served += static_cast<int>(clients.size());
        Route route(data, 0);
        route.set_clients(clients);
        CHECK(route.load_feasible());
        CHECK(route.tw_feasible());
        total += route.distance();
    }
    CHECK(served == kNumNodes - 1);
    return total;
}

}  // namespace

TEST_CASE("C101's published routes price at the exact-method 827.3 under floor(10 * d)",
          "[routing][solomon]") {
    auto data = build_c101(10, [](Row const& a, Row const& b) {
        return static_cast<Measure>(std::floor(10.0 * euclid(a, b)));
    });
    CHECK(measures_match(reprice(data), static_cast<Measure>(8273)));
}

TEST_CASE("C101's published routes price at the best-known 828.94 in double precision",
          "[routing][solomon]") {
    if constexpr (std::is_integral_v<Measure>) {
        SKIP("SINTEF's double-precision table needs COSO_MEASURE=double (#249)");
    } else {
        auto data = build_c101(
            1, [](Row const& a, Row const& b) { return static_cast<Measure>(euclid(a, b)); });
        CHECK(std::lround(static_cast<double>(reprice(data)) * 100.0) == 82894);
    }
}

TEST_CASE("A distance budget is decided on the true arc lengths in the double build",
          "[routing][solomon]") {
    if constexpr (std::is_integral_v<Measure>) {
        SKIP("Chao TOP's three-decimal coordinates against tmax need COSO_MEASURE=double (#249)");
    } else {
        auto fits = [](double x) {
            ProblemData::Builder b;
            b.add_depot({0.0, 0.0});
            b.add_client({x, 0.0});
            VehicleTypeParams v;
            v.max_distance = static_cast<Measure>(25.0);
            b.add_vehicle_type(1, v);
            auto data = b.build(0);
            Route route(data, 0);
            route.set_clients({0});
            return route.dist_feasible();
        };
        // tmax 25.0: exactly on the budget fits, and 0.002 over it does not. Rounding each arc to
        // an integer would price both round trips at 26.
        CHECK(fits(12.5));
        CHECK_FALSE(fits(12.501));
    }
}

TEST_CASE("Rounding accumulated over irrational arcs is not a time window violation",
          "[routing][solomon]") {
    if constexpr (std::is_integral_v<Measure>) {
        SKIP("Integer measures accumulate no rounding");
    } else {
        // The last client's deadline is the sum of the arcs, in a different order from the route's.
        std::vector<Coord> const at = {{0.3, 0.1}, {0.7, 1.9}, {2.3, 0.2}, {3.1, 3.7}, {5.9, 0.4}};
        auto arc = [&](Coord a, Coord b) { return std::hypot(a.x - b.x, a.y - b.y); };
        double deadline = 0.0;
        for (std::size_t i = at.size() - 1; i > 0; --i) {
            deadline += arc(at[i - 1], at[i]);
        }
        deadline += arc({0.0, 0.0}, at[0]);

        auto route_is_on_time = [&](double close) {
            ProblemData::Builder b;
            b.add_depot({0.0, 0.0});
            for (std::size_t i = 0; i < at.size(); ++i) {
                ClientParams c;
                if (i + 1 == at.size()) {
                    c.tw = TimeWindow{0, static_cast<Measure>(close)};
                }
                b.add_client(at[i], c);
            }
            b.add_vehicle_type(1);
            auto data = b.build(0);
            Route route(data, 0);
            route.set_clients({0, 1, 2, 3, 4});
            return route.tw_feasible();
        };
        CHECK(route_is_on_time(deadline));
        CHECK_FALSE(route_is_on_time(deadline - 1e-3));
    }
}

TEST_CASE("Comparison helpers are exact in integer builds and tolerant in the double build",
          "[routing][measure]") {
    CHECK(is_improving(static_cast<Cost>(-1)));
    CHECK_FALSE(is_improving(static_cast<Cost>(0)));
    CHECK_FALSE(is_improving(static_cast<Cost>(1)));
    CHECK(is_zero_excess(static_cast<Measure>(0)));
    CHECK_FALSE(is_zero_excess(static_cast<Measure>(1)));
    CHECK(measures_match(static_cast<Measure>(3), static_cast<Measure>(3)));
    CHECK_FALSE(measures_match(static_cast<Measure>(3), static_cast<Measure>(4)));
    CHECK(costs_match(static_cast<Cost>(3), static_cast<Cost>(3)));
    CHECK_FALSE(costs_match(static_cast<Cost>(3), static_cast<Cost>(4)));
    if constexpr (std::is_floating_point_v<Cost>) {
        CHECK_FALSE(is_improving(-1e-12));  // rounding, or a descent could cycle on it
        CHECK(is_zero_excess(1e-12));
        CHECK(measures_match(1.0, 1.0 + 1e-12));
        CHECK(costs_match(1.0, 1.0 + 1e-12));
        CHECK_FALSE(costs_match(1.0, 1.0 + 1e-3));
        // A penalised total this large recomputes differently by more than the absolute tolerance.
        CHECK(costs_match(-4.39469e7, -4.39469e7 + 1.1e-6));
    }
}

TEST_CASE("A search on C101 keeps its predicted deltas in step with its costs",
          "[routing][solomon]") {
    // Heavy time warp makes for large penalised costs.  In a build with assertions the local search
    // checks each delta it applies, which is what this run exercises in the double build.
    RoutingModel model;
    DepotParams depot;
    depot.tw = TimeWindow{static_cast<Measure>(kC101[0].ready), static_cast<Measure>(kC101[0].due)};
    model.add_depot(kC101[0].x, kC101[0].y, depot);
    for (int i = 1; i < kNumNodes; ++i) {
        Row const& r = kC101[i];
        ClientParams c;
        c.demand = {r.demand};
        c.tw = TimeWindow{static_cast<Measure>(r.ready), static_cast<Measure>(r.due)};
        c.service = static_cast<Measure>(r.service);
        model.add_client(r.x, r.y, c);
    }
    VehicleTypeParams v;
    v.capacity = {kCapacity};
    model.add_vehicle_type(kVehicles, v);
    Result r = model.solve(TimeLimit(0.0, 0.5));
    CHECK(r.unserved().empty());
}

TEST_CASE("CVRPLIB EUC_2D stays rounded to the nearest integer in every build",
          "[routing][solomon]") {
    // The format defines the rounding, and CVRPLIB's published values assume it: one client at
    // (1, 1) is a round trip of 2 * nint(1.41...) = 2, whatever Measure is.
    auto path = std::filesystem::temp_directory_path() /
                ("coso_euc2d_" + std::to_string(::getpid()) + ".vrp");
    {
        std::ofstream out(path);
        out << "NAME : t\nTYPE : CVRP\nDIMENSION : 2\nEDGE_WEIGHT_TYPE : EUC_2D\nCAPACITY : 10\n"
               "NODE_COORD_SECTION\n1 0 0\n2 1 1\nDEMAND_SECTION\n1 0\n2 1\nDEPOT_SECTION\n1\n-1\n"
               "EOF\n";
    }
    Result r = solve(path.string(), TimeLimit(0.0, 0.001));
    std::filesystem::remove(path);
    CHECK(r.distance() == 2.0);
}
