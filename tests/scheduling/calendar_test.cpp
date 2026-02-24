#include <catch2/catch_test_macros.hpp>

#include "scheduling/calendar.h"
#include "scheduling/schedule_data.h"

using namespace coso;

// ---------------------------------------------------------------------------
//  MachineCalendar basics
// ---------------------------------------------------------------------------

TEST_CASE("MachineCalendar: default construction", "[scheduling][calendar]")
{
    MachineCalendar cal;
    CHECK(cal.num_machines() == 0);
}

TEST_CASE("MachineCalendar: no restrictions means always available",
          "[scheduling][calendar]")
{
    MachineCalendar cal(3);

    CHECK_FALSE(cal.has_calendar(0));
    CHECK(cal.available(0, 0, 100));
    CHECK(cal.available(1, 50, 200));
    CHECK(cal.next_available(0, 0) == 0);
    CHECK(cal.next_available(2, 42, 10) == 42);
}

TEST_CASE("MachineCalendar: single availability window",
          "[scheduling][calendar]")
{
    MachineCalendar cal(2);

    // Machine 0: available [10, 50)
    cal.add_available(0, 10, 50);

    CHECK(cal.has_calendar(0));
    CHECK_FALSE(cal.has_calendar(1));

    // Fully within window.
    CHECK(cal.available(0, 10, 50));
    CHECK(cal.available(0, 15, 30));
    CHECK(cal.available(0, 10, 11));

    // Before window.
    CHECK_FALSE(cal.available(0, 0, 5));
    CHECK_FALSE(cal.available(0, 5, 15));

    // After window.
    CHECK_FALSE(cal.available(0, 45, 55));
    CHECK_FALSE(cal.available(0, 50, 60));
}

TEST_CASE("MachineCalendar: next_available", "[scheduling][calendar]")
{
    MachineCalendar cal(1);

    // Machine 0: available [10, 30) and [50, 80)
    cal.add_available(0, 10, 30);
    cal.add_available(0, 50, 80);

    // Before first window.
    CHECK(cal.next_available(0, 0, 1) == 10);
    CHECK(cal.next_available(0, 0, 5) == 10);
    CHECK(cal.next_available(0, 0, 20) == 10);  // fits in [10,30)

    // Too large for first window, fits in second.
    CHECK(cal.next_available(0, 0, 25) == 50);

    // Within first window.
    CHECK(cal.next_available(0, 15, 5) == 15);
    CHECK(cal.next_available(0, 15, 15) == 15);  // 15+15=30, fits
    CHECK(cal.next_available(0, 15, 16) == 50);  // 15+16=31, doesn't fit

    // In the gap.
    CHECK(cal.next_available(0, 35, 1) == 50);

    // Within second window.
    CHECK(cal.next_available(0, 60, 10) == 60);

    // Too large for anything.
    CHECK(cal.next_available(0, 0, 40) == -1);

    // Past all windows.
    CHECK(cal.next_available(0, 100, 1) == -1);
}

TEST_CASE("MachineCalendar: overlapping intervals merge",
          "[scheduling][calendar]")
{
    MachineCalendar cal(1);

    cal.add_available(0, 10, 30);
    cal.add_available(0, 20, 50);  // overlaps with [10,30)

    // Should be merged to [10, 50).
    CHECK(cal.available(0, 10, 50));
    CHECK(cal.available(0, 25, 40));
    CHECK_FALSE(cal.available(0, 5, 15));
    CHECK_FALSE(cal.available(0, 45, 55));
}

TEST_CASE("MachineCalendar: adjacent intervals merge",
          "[scheduling][calendar]")
{
    MachineCalendar cal(1);

    cal.add_available(0, 10, 30);
    cal.add_available(0, 30, 50);  // adjacent

    // Should be merged to [10, 50).
    CHECK(cal.available(0, 10, 50));
    CHECK(cal.available(0, 29, 31));
}

TEST_CASE("MachineCalendar: empty interval ignored", "[scheduling][calendar]")
{
    MachineCalendar cal(1);

    cal.add_available(0, 10, 10);  // empty [10, 10)
    CHECK_FALSE(cal.has_calendar(0));

    cal.add_available(0, 20, 15);  // negative range
    CHECK_FALSE(cal.has_calendar(0));
}

TEST_CASE("MachineCalendar: multiple machines independent",
          "[scheduling][calendar]")
{
    MachineCalendar cal(2);

    cal.add_available(0, 0, 100);
    cal.add_available(1, 200, 300);

    CHECK(cal.available(0, 0, 100));
    CHECK_FALSE(cal.available(0, 200, 300));

    CHECK_FALSE(cal.available(1, 0, 100));
    CHECK(cal.available(1, 200, 300));
}

TEST_CASE("MachineCalendar: out-of-range machine", "[scheduling][calendar]")
{
    MachineCalendar cal(2);

    CHECK_FALSE(cal.available(-1, 0, 10));
    CHECK_FALSE(cal.available(5, 0, 10));
    CHECK(cal.next_available(-1, 0) == -1);
    CHECK_FALSE(cal.has_calendar(5));

    CHECK_THROWS(cal.add_available(-1, 0, 10));
    CHECK_THROWS(cal.add_available(5, 0, 10));
}

// ---------------------------------------------------------------------------
//  Integration with ScheduleData
// ---------------------------------------------------------------------------

TEST_CASE("ScheduleData: calendar via Builder", "[scheduling][calendar]")
{
    ScheduleData::Builder builder;

    builder.add_machine({.name = "M0"});
    builder.add_machine({.name = "M1"});
    builder.add_job({.name = "J0"});
    builder.add_operation(0, {.machine = 0, .duration = 5});

    // Machine 0: available [0, 100), Machine 1: available [50, 150)
    builder.add_machine_available(0, 0, 100);
    builder.add_machine_available(1, 50, 150);

    auto data = builder.build();

    CHECK(data.has_calendar());

    auto const& cal = data.calendar();
    CHECK(cal.available(0, 0, 100));
    CHECK_FALSE(cal.available(0, 50, 150));

    CHECK_FALSE(cal.available(1, 0, 50));
    CHECK(cal.available(1, 50, 150));
}

TEST_CASE("ScheduleData: no calendar by default", "[scheduling][calendar]")
{
    ScheduleData::Builder builder;
    builder.add_machine({.name = "M0"});
    builder.add_job({.name = "J0"});
    builder.add_operation(0, {.machine = 0, .duration = 3});

    auto data = builder.build();

    CHECK_FALSE(data.has_calendar());
}
