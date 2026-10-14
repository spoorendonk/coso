#include "rostering/parsers.h"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using namespace coso;

// ---------------------------------------------------------------------------
//  NRP parser
// ---------------------------------------------------------------------------

TEST_CASE("parse_nrp - small instance", "[rostering][parser]") {
    // A small NRP instance with:
    //   - 7-day horizon
    //   - 2 shift types: D (day, 480 min) and N (night, 480 min), N cannot
    //     follow N
    //   - 3 employees
    //   - Days off, shift requests, and cover requirements
    std::string content = R"(
SECTION_HORIZON
7

SECTION_SHIFTS
D,480
N,480,N

SECTION_STAFF
Alice,MaxShifts=5,MaxTotalMinutes=2400,MinTotalMinutes=960,MaxConsecutiveShifts=5,MinConsecutiveShifts=2,MinConsecutiveDaysOff=2,MaxWeekends=1
Bob,MaxShifts=5,MaxTotalMinutes=2400,MinTotalMinutes=480,MaxConsecutiveShifts=4,MinConsecutiveShifts=1,MinConsecutiveDaysOff=2,MaxWeekends=1
Carol,MaxShifts=4,MaxTotalMinutes=1920,MaxConsecutiveShifts=3,MinConsecutiveShifts=1,MinConsecutiveDaysOff=2,MaxWeekends=2

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
    CHECK(data.shift_types[0].duration_minutes == 480);
    CHECK(data.shift_types[1].name == "N");
    CHECK(data.shift_types[1].duration_minutes == 480);

    // Employees.
    REQUIRE(data.num_employees() == 3);
    CHECK(data.employees[0].name == "Alice");
    CHECK(data.employees[1].name == "Bob");
    CHECK(data.employees[2].name == "Carol");

    // MaxConsecutiveShifts.
    CHECK(data.employees[0].max_consecutive_days == 5);
    CHECK(data.employees[1].max_consecutive_days == 4);
    CHECK(data.employees[2].max_consecutive_days == 3);

    // MaxWeekends and the horizon-scoped working-time bounds, in the named
    // key=value dialect (#203 v1 EXTEND items 2 and 3).  Carol declares no
    // MinTotalMinutes, so hers stays at the inert default of 0.
    CHECK(data.employees[0].max_weekends == 1);
    CHECK(data.employees[1].max_weekends == 1);
    CHECK(data.employees[2].max_weekends == 2);
    CHECK(data.employees[0].max_total_minutes == 2400);
    CHECK(data.employees[1].max_total_minutes == 2400);
    CHECK(data.employees[2].max_total_minutes == 1920);
    CHECK(data.employees[0].min_total_minutes == 960);
    CHECK(data.employees[1].min_total_minutes == 480);
    CHECK(data.employees[2].min_total_minutes == 0);

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

TEST_CASE("parse_nrp - error on empty input", "[rostering][parser]") {
    // Empty input should parse but yield empty data (horizon 0, no shifts, etc.)
    auto data = parse_nrp("");
    CHECK(data.horizon == 0);
    CHECK(data.num_shift_types() == 0);
    CHECK(data.num_employees() == 0);
}

TEST_CASE("parse_nrp - minimal instance", "[rostering][parser]") {
    // Absolute minimal: 1 shift, 1 employee, 1 day.
    std::string content = R"(
SECTION_HORIZON
1

SECTION_SHIFTS
D,480

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

    auto dem = data.get_demand(0, 0);
    CHECK(dem.min_employees == 1);
}

TEST_CASE("parse_nrp - a STAFF line in neither dialect is rejected", "[rostering][parser]") {
    // #236: the dialect used to be guessed from the token count, so these
    // lines parsed as a different employee instead of failing.
    auto with_staff = [](std::string const& staff) {
        return "SECTION_HORIZON\n7\n\nSECTION_SHIFTS\nD,480\n\nSECTION_STAFF\n" + staff +
               "\n\nSECTION_COVER\n0,D,1,100,1\n";
    };

    SECTION("the full positional line still parses") {
        auto data = parse_nrp(with_staff("A,D=7,4320,3390,5,2,2,1"));
        REQUIRE(data.num_employees() == 1);
        CHECK(data.employees[0].max_total_minutes == 4320);
        CHECK(data.employees[0].min_total_minutes == 3390);
        CHECK(data.employees[0].max_consecutive_days == 5);
        CHECK(data.employees[0].max_weekends == 1);
    }

    SECTION("an empty positional field no longer shifts the columns left") {
        CHECK_THROWS_AS(parse_nrp(with_staff("A,,4320,3390,5,2,2,1")), std::runtime_error);
    }

    SECTION("a short positional line no longer becomes an unconstrained employee") {
        CHECK_THROWS_AS(parse_nrp(with_staff("B,D=7,4320,3390")), std::runtime_error);
    }

    SECTION("a positional line with extra fields") {
        CHECK_THROWS_AS(parse_nrp(with_staff("A,D=7,4320,3390,5,2,2,1,9")), std::runtime_error);
    }

    SECTION("a key=value field in the MaxShifts column") {
        CHECK_THROWS_AS(parse_nrp(with_staff("A,MaxTotalMinutes=10,4320,3390,5,2,2,1")),
                        std::runtime_error);
    }

    SECTION("a MaxShifts cap for an undeclared shift") {
        CHECK_THROWS_AS(parse_nrp(with_staff("A,X=7,4320,3390,5,2,2,1")), std::runtime_error);
    }

    SECTION("a positional field that is not an integer") {
        CHECK_THROWS_AS(parse_nrp(with_staff("A,D=7,4320x,3390,5,2,2,1")), std::runtime_error);
    }

    SECTION("a key=value line with a bare value") {
        CHECK_THROWS_AS(parse_nrp(with_staff("A,MaxTotalMinutes=4320,3390")), std::runtime_error);
    }

    SECTION("an unknown key, which would otherwise drop a misspelled bound") {
        CHECK_THROWS_AS(parse_nrp(with_staff("A,MaxTotalMinute=4320")), std::runtime_error);
    }

    SECTION("a repeated key") {
        CHECK_THROWS_AS(parse_nrp(with_staff("A,MaxWeekends=1,MaxWeekends=2")), std::runtime_error);
    }

    SECTION("an empty key=value field") {
        CHECK_THROWS_AS(parse_nrp(with_staff("A,MaxWeekends=1,,MaxTotalMinutes=10")),
                        std::runtime_error);
    }

    SECTION("a repeated employee") {
        CHECK_THROWS_AS(parse_nrp(with_staff("A,MaxWeekends=1\nA,MaxWeekends=2")),
                        std::runtime_error);
    }
}

TEST_CASE("parse_nrp - a repeated COVER line is rejected", "[rostering][parser]") {
    // #236: the second line for the same (day, shift) used to win silently.
    std::string const content =
        "SECTION_HORIZON\n7\n\nSECTION_SHIFTS\nD,480\n\nSECTION_STAFF\nA,MaxWeekends=1\n\n"
        "SECTION_COVER\n0,D,1,100,1\n0,D,3,100,1\n";
    CHECK_THROWS_AS(parse_nrp(content), std::runtime_error);
}
