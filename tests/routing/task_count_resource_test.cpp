#include "routing/resources/task_count_resource.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  Helper: build a small instance for testing.
// ---------------------------------------------------------------------------

/// 1 depot, 5 clients, vehicle type with min_tasks=2, max_tasks=4.
static ProblemData make_task_count_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {100}, .min_tasks = 2, .max_tasks = 4});

    b.add_client({10.0, 0.0}, {.demand = {1}});  // client 0
    b.add_client({20.0, 0.0}, {.demand = {1}});  // client 1
    b.add_client({30.0, 0.0}, {.demand = {1}});  // client 2
    b.add_client({40.0, 0.0}, {.demand = {1}});  // client 3
    b.add_client({50.0, 0.0}, {.demand = {1}});  // client 4

    return b.build(0);
}

/// Vehicle type with max_tasks only (no minimum).
static ProblemData make_max_only_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(2, {.capacity = {100}, .max_tasks = 3});

    b.add_client({10.0, 0.0}, {.demand = {1}});
    b.add_client({20.0, 0.0}, {.demand = {1}});
    b.add_client({30.0, 0.0}, {.demand = {1}});
    b.add_client({40.0, 0.0}, {.demand = {1}});

    return b.build(0);
}

/// Vehicle type with no task constraints (unlimited).
static ProblemData make_unlimited_instance() {
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(1, {.capacity = {100}});

    b.add_client({10.0, 0.0}, {.demand = {1}});
    b.add_client({20.0, 0.0}, {.demand = {1}});

    return b.build(0);
}

// ===========================================================================
//  Init tests
// ===========================================================================

TEST_CASE("TaskCountResource::init creates count=1 state", "[task_count_resource]") {
    auto data = make_task_count_instance();

    auto s0 = TaskCountResource::init(data, 0);
    CHECK(s0.count == 1);

    auto s4 = TaskCountResource::init(data, 4);
    CHECK(s4.count == 1);
}

TEST_CASE("TaskCountResource::init_depot creates count=0 state", "[task_count_resource]") {
    auto data = make_task_count_instance();
    auto s = TaskCountResource::init_depot(data);
    CHECK(s.count == 0);
}

// ===========================================================================
//  Merge tests
// ===========================================================================

TEST_CASE("TaskCountResource::merge combines counts", "[task_count_resource]") {
    auto data = make_task_count_instance();

    auto s0 = TaskCountResource::init(data, 0);
    auto s1 = TaskCountResource::init(data, 1);
    auto merged = TaskCountResource::merge(s0, s1);
    CHECK(merged.count == 2);

    auto s2 = TaskCountResource::init(data, 2);
    auto merged3 = TaskCountResource::merge(merged, s2);
    CHECK(merged3.count == 3);
}

TEST_CASE("TaskCountResource::merge with depot state", "[task_count_resource]") {
    auto data = make_task_count_instance();

    auto depot = TaskCountResource::init_depot(data);
    auto s0 = TaskCountResource::init(data, 0);

    auto merged = TaskCountResource::merge(depot, s0);
    CHECK(merged.count == 1);

    auto merged2 = TaskCountResource::merge(s0, depot);
    CHECK(merged2.count == 1);
}

TEST_CASE("TaskCountResource::merge_reverse equals merge", "[task_count_resource]") {
    auto data = make_task_count_instance();

    auto s0 = TaskCountResource::init(data, 0);
    auto s1 = TaskCountResource::init(data, 1);

    auto fwd = TaskCountResource::merge(s0, s1);
    auto rev = TaskCountResource::merge_reverse(s0, s1);

    CHECK(fwd.count == rev.count);
}

// ===========================================================================
//  Excess tests
// ===========================================================================

TEST_CASE("TaskCountResource::excess with min and max constraints", "[task_count_resource]") {
    auto data = make_task_count_instance();
    auto const& vt = data.vehicle_type(0);  // min=2, max=4

    SECTION("1 client: below minimum") {
        auto s = TaskCountResource::init(data, 0);
        // count=1, min=2 -> excess=1
        CHECK(TaskCountResource::excess(s, vt) == 1);
    }

    SECTION("2 clients: at minimum, no excess") {
        auto s0 = TaskCountResource::init(data, 0);
        auto s1 = TaskCountResource::init(data, 1);
        auto merged = TaskCountResource::merge(s0, s1);
        CHECK(TaskCountResource::excess(merged, vt) == 0);
    }

    SECTION("3 clients: within range, no excess") {
        auto s0 = TaskCountResource::init(data, 0);
        auto s1 = TaskCountResource::init(data, 1);
        auto s2 = TaskCountResource::init(data, 2);
        auto m = TaskCountResource::merge(TaskCountResource::merge(s0, s1), s2);
        CHECK(TaskCountResource::excess(m, vt) == 0);
    }

    SECTION("4 clients: at maximum, no excess") {
        auto s0 = TaskCountResource::init(data, 0);
        auto s1 = TaskCountResource::init(data, 1);
        auto s2 = TaskCountResource::init(data, 2);
        auto s3 = TaskCountResource::init(data, 3);
        auto m = TaskCountResource::merge(TaskCountResource::merge(s0, s1),
                                          TaskCountResource::merge(s2, s3));
        CHECK(TaskCountResource::excess(m, vt) == 0);
    }

    SECTION("5 clients: above maximum") {
        auto s0 = TaskCountResource::init(data, 0);
        auto s1 = TaskCountResource::init(data, 1);
        auto s2 = TaskCountResource::init(data, 2);
        auto s3 = TaskCountResource::init(data, 3);
        auto s4 = TaskCountResource::init(data, 4);
        auto m =
            TaskCountResource::merge(TaskCountResource::merge(TaskCountResource::merge(s0, s1),
                                                              TaskCountResource::merge(s2, s3)),
                                     s4);
        // count=5, max=4 -> excess=1
        CHECK(TaskCountResource::excess(m, vt) == 1);
    }
}

TEST_CASE("TaskCountResource::excess with max-only constraint", "[task_count_resource]") {
    auto data = make_max_only_instance();
    auto const& vt = data.vehicle_type(0);  // min=0, max=3

    SECTION("1 client: no minimum constraint, no excess") {
        auto s = TaskCountResource::init(data, 0);
        CHECK(TaskCountResource::excess(s, vt) == 0);
    }

    SECTION("4 clients: above maximum") {
        auto s0 = TaskCountResource::init(data, 0);
        auto s1 = TaskCountResource::init(data, 1);
        auto s2 = TaskCountResource::init(data, 2);
        auto s3 = TaskCountResource::init(data, 3);
        auto m = TaskCountResource::merge(TaskCountResource::merge(s0, s1),
                                          TaskCountResource::merge(s2, s3));
        // count=4, max=3 -> excess=1
        CHECK(TaskCountResource::excess(m, vt) == 1);
    }
}

TEST_CASE("TaskCountResource::excess with no constraints (unlimited)", "[task_count_resource]") {
    auto data = make_unlimited_instance();
    auto const& vt = data.vehicle_type(0);  // min=0, max=0

    auto s0 = TaskCountResource::init(data, 0);
    auto s1 = TaskCountResource::init(data, 1);
    auto merged = TaskCountResource::merge(s0, s1);

    // No constraints -> always 0 excess.
    CHECK(TaskCountResource::excess(merged, vt) == 0);
}

TEST_CASE("TaskCountResource::excess with empty route", "[task_count_resource]") {
    auto data = make_task_count_instance();
    auto const& vt = data.vehicle_type(0);  // min=2, max=4

    auto depot = TaskCountResource::init_depot(data);
    // Empty route: count=0, min=2 -> excess=2
    CHECK(TaskCountResource::excess(depot, vt) == 2);
}

TEST_CASE("TaskCountResource::excess convenience overload", "[task_count_resource]") {
    auto data = make_task_count_instance();

    auto s0 = TaskCountResource::init(data, 0);

    // count=1, min=3, max=5 -> below min by 2
    CHECK(TaskCountResource::excess(s0, 3, 5) == 2);

    // count=1, min=0, max=0 -> no constraints
    CHECK(TaskCountResource::excess(s0, 0, 0) == 0);

    auto s1 = TaskCountResource::init(data, 1);
    auto s2 = TaskCountResource::init(data, 2);
    auto m = TaskCountResource::merge(TaskCountResource::merge(s0, s1), s2);

    // count=3, min=1, max=2 -> above max by 1
    CHECK(TaskCountResource::excess(m, 1, 2) == 1);
}
