#include "assignment/parsers.h"

#include <catch2/catch_test_macros.hpp>

using namespace coso;

// ---------------------------------------------------------------------------
//  NRP parser
// ---------------------------------------------------------------------------

TEST_CASE("parse_nrp - small instance", "[assignment][parser]") {
    // A small NRP instance with:
    //   - 7-day horizon
    //   - 2 shift types: D (day, 8h) and N (night, 8h), N cannot follow N
    //   - 3 employees
    //   - Days off, shift requests, and cover requirements
    std::string content = R"(
SECTION_HORIZON
7

SECTION_SHIFTS
D,8
N,8,N

SECTION_STAFF
Alice,MaxShifts=5,MaxTotalMinutes=2400,MaxConsecutiveShifts=5,MinConsecutiveShifts=2,MinConsecutiveDaysOff=2,MaxWeekends=1
Bob,MaxShifts=5,MaxTotalMinutes=2400,MaxConsecutiveShifts=4,MinConsecutiveShifts=1,MinConsecutiveDaysOff=2,MaxWeekends=1
Carol,MaxShifts=4,MaxTotalMinutes=1920,MaxConsecutiveShifts=3,MinConsecutiveShifts=1,MinConsecutiveDaysOff=2,MaxWeekends=1

SECTION_DAYS_OFF
Alice,0,6
Bob,5

SECTION_SHIFT_ON_REQUESTS
Alice,1,D,3
Bob,2,N,2

SECTION_SHIFT_OFF_REQUESTS
Carol,3,N,4

SECTION_COVER
0,D,2,1,1
0,N,1,1,1
1,D,2,1,1
1,N,1,1,1
2,D,2,1,1
2,N,1,1,1
3,D,2,1,1
3,N,1,1,1
4,D,2,1,1
4,N,1,1,1
5,D,1,1,1
5,N,1,1,1
6,D,1,1,1
6,N,1,1,1
)";

    auto data = parse_nrp(content);

    // Horizon.
    CHECK(data.horizon == 7);

    // Shift types.
    REQUIRE(data.num_shift_types() == 2);
    CHECK(data.shift_types[0].name == "D");
    CHECK(data.shift_types[0].duration_hours == 8);
    CHECK(data.shift_types[1].name == "N");
    CHECK(data.shift_types[1].duration_hours == 8);

    // Employees.
    REQUIRE(data.num_employees() == 3);
    CHECK(data.employees[0].name == "Alice");
    CHECK(data.employees[1].name == "Bob");
    CHECK(data.employees[2].name == "Carol");

    // MaxTotalMinutes=2400 -> 40 hours.
    CHECK(data.employees[0].max_hours_per_week == 40);
    CHECK(data.employees[1].max_hours_per_week == 40);
    // MaxTotalMinutes=1920 -> 32 hours.
    CHECK(data.employees[2].max_hours_per_week == 32);

    // MaxConsecutiveShifts.
    CHECK(data.employees[0].max_consecutive_days == 5);
    CHECK(data.employees[1].max_consecutive_days == 4);
    CHECK(data.employees[2].max_consecutive_days == 3);

    // Days off: Alice off on days 0, 6; Bob off on day 5.
    CHECK(data.is_unavailable(0, 0));        // Alice, day 0
    CHECK(data.is_unavailable(0, 6));        // Alice, day 6
    CHECK_FALSE(data.is_unavailable(0, 3));  // Alice, day 3 is fine
    CHECK(data.is_unavailable(1, 5));        // Bob, day 5
    CHECK_FALSE(data.is_unavailable(1, 0));  // Bob, day 0 is fine
    CHECK_FALSE(data.is_unavailable(2, 0));  // Carol has no days off

    // Shift on requests: Alice wants D on day 1 (weight 3),
    //                    Bob wants N on day 2 (weight 2).
    // Shift off requests: Carol avoids N on day 3 (weight -4).
    REQUIRE(data.preferences.size() == 3);

    // On-requests come first in parsing order.
    CHECK(data.preferences[0].employee == 0);  // Alice
    CHECK(data.preferences[0].day == 1);
    CHECK(data.preferences[0].shift_type == 0);  // D
    CHECK(data.preferences[0].weight == 3);

    CHECK(data.preferences[1].employee == 1);  // Bob
    CHECK(data.preferences[1].day == 2);
    CHECK(data.preferences[1].shift_type == 1);  // N
    CHECK(data.preferences[1].weight == 2);

    CHECK(data.preferences[2].employee == 2);  // Carol
    CHECK(data.preferences[2].day == 3);
    CHECK(data.preferences[2].shift_type == 1);  // N
    CHECK(data.preferences[2].weight == -4);     // off-request -> negative

    // Cover requirements: day 0, shift D -> 2 required.
    auto d0_d = data.get_demand(0, 0);  // shift D (idx 0), day 0
    CHECK(d0_d.min_employees == 2);

    auto d0_n = data.get_demand(1, 0);  // shift N (idx 1), day 0
    CHECK(d0_n.min_employees == 1);

    // Weekend days (5, 6) have lower cover.
    auto d5_d = data.get_demand(0, 5);
    CHECK(d5_d.min_employees == 1);

    // Forbidden sequences: N cannot follow N.
    REQUIRE(data.forbidden_sequences.size() == 1);
    CHECK(data.forbidden_sequences[0][0] == 1);  // N
    CHECK(data.forbidden_sequences[0][1] == 1);  // N
}

TEST_CASE("parse_nrp - error on empty input", "[assignment][parser]") {
    // Empty input should parse but yield empty data (horizon 0, no shifts, etc.)
    auto data = parse_nrp("");
    CHECK(data.horizon == 0);
    CHECK(data.num_shift_types() == 0);
    CHECK(data.num_employees() == 0);
}

TEST_CASE("parse_nrp - minimal instance", "[assignment][parser]") {
    // Absolute minimal: 1 shift, 1 employee, 1 day.
    std::string content = R"(
SECTION_HORIZON
1

SECTION_SHIFTS
D,8

SECTION_STAFF
E0,MaxShifts=1,MaxTotalMinutes=480,MaxConsecutiveShifts=1,MinConsecutiveShifts=1,MinConsecutiveDaysOff=1,MaxWeekends=0

SECTION_COVER
0,D,1,1,1
)";

    auto data = parse_nrp(content);

    CHECK(data.horizon == 1);
    REQUIRE(data.num_shift_types() == 1);
    REQUIRE(data.num_employees() == 1);
    CHECK(data.employees[0].name == "E0");
    CHECK(data.employees[0].max_hours_per_week == 8);

    auto dem = data.get_demand(0, 0);
    CHECK(dem.min_employees == 1);
}
