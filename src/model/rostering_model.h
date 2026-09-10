#pragma once

#include "types.h"

#include <climits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace coso {

/// Parameters for a shift type.
struct ShiftTypeParams {
    std::string name;
    /// Shift length in **minutes**, as every verified benchmark format carries
    /// it: schedulingbenchmarks.org NRP's "Length in mins" (480 for an 8h
    /// shift) and the .ros <Duration> element. See the v1 scope ruling on #203.
    int duration_minutes = 0;
};

/// Parameters for an employee.
struct EmployeeParams {
    std::string name;
    std::vector<std::string> skills;
    int max_consecutive_days = 5;

    /// Maximum number of distinct weekends the employee may work over the
    /// horizon. Default INT_MAX = unlimited.
    ///
    /// Weekend convention: day index `d` falls in a weekend iff `d % 7` is 5
    /// or 6, i.e. the horizon starts on a Monday, which is what SB-NRP
    /// assumes. Weekend `w` is the day pair (7w+5, 7w+6); an employee "works"
    /// that weekend if assigned any shift on either day, and the count is of
    /// distinct weekends worked, not of weekend days.
    ///
    /// SB-NRP STAFF column 8 (`MaxWeekends`); INRC-II `maxWorkingWeekends`.
    int max_weekends = INT_MAX;

    /// Minimum total working time over the **whole horizon**, in minutes.
    /// Default 0 = no lower bound. SB-NRP `MinTotalMinutes` (> 0 in all 24
    /// instances); INRC-II `minAssign`; NSPLib case files.
    int min_total_minutes = 0;

    /// Maximum total working time over the **whole horizon**, in minutes.
    /// Default INT_MAX = unlimited. SB-NRP `MaxTotalMinutes`; INRC-II
    /// `maxAssign`; NSPLib case files.
    ///
    /// An employee's total is the sum of `ShiftTypeParams::duration_minutes`
    /// over the days they are assigned -- this pair of bounds is what gives
    /// the declared shift duration its first reader, which is the substance
    /// of #233.
    int max_total_minutes = INT_MAX;
};

/// Demand parameters for a shift on a given day.
struct DemandParams {
    int min_employees = 0;
    int max_employees = INT_MAX;
    std::string required_skill;  ///< empty = no skill requirement
};

/// Rostering model: declare employees, shift types, coverage demand, constraints, then solve.
///
/// Employees x days x shift types. What it can express, and what each engine does
/// with it, is specified by the v1 scope ruling on #203.
class RosteringModel {
public:
    /// One demand entry: (shift_type, day) -> DemandParams.
    struct DemandEntry {
        int shift_type;
        int day;
        DemandParams params;
    };

    /// One preference entry.
    struct PrefEntry {
        int employee;
        int day;
        int shift_type;
        int weight;
    };

    /// One unavailability entry.
    struct UnavailEntry {
        int employee;
        int day;
    };

    /// Add a shift type.
    int add_shift_type(ShiftTypeParams p);

    /// Add an employee.
    int add_employee(EmployeeParams p);

    // -- Planning horizon ----------------------------------------------------

    /// Set the planning horizon in days.
    void set_horizon(int days);

    // -- Demand --------------------------------------------------------------

    /// Add a demand for a specific shift type on a specific day.
    void add_demand(int shift_type, int day, DemandParams p);

    // -- Constraints (hard) --------------------------------------------------

    /// Forbid `second` from directly following `first` on the next day.
    void add_forbidden_sequence(int first, int second);

    // -- Preferences (soft) --------------------------------------------------

    /// Add a preference weight for an employee on a specific day/shift.
    void add_preference(int employee, int day, int shift_type, int weight);

    /// Mark an employee as unavailable on a specific day.
    void add_unavailability(int employee, int day);

    // -- Solve ---------------------------------------------------------------

    /// Solve the rostering problem within the given time limit.
    Result solve(TimeLimit tl);

    // -- Accessors -----------------------------------------------------------

    [[nodiscard]] int num_shift_types() const noexcept {
        return static_cast<int>(shift_types_.size());
    }
    [[nodiscard]] ShiftTypeParams const& shift_type(int s) const {
        if (s < 0 || static_cast<size_t>(s) >= shift_types_.size()) {
            throw std::out_of_range("RosteringModel::shift_type: invalid index");
        }
        return shift_types_[s];
    }

    [[nodiscard]] int num_employees() const noexcept { return static_cast<int>(employees_.size()); }
    [[nodiscard]] EmployeeParams const& employee(int e) const {
        if (e < 0 || static_cast<size_t>(e) >= employees_.size()) {
            throw std::out_of_range("RosteringModel::employee: invalid index");
        }
        return employees_[e];
    }

    [[nodiscard]] int horizon() const noexcept { return horizon_; }

    /// Per-day demands from add_demand(shift_type, day, p), in declaration
    /// order: no dedup, no range check.
    [[nodiscard]] auto const& demands() const noexcept { return demands_; }

    /// Forbidden (first, second) shift-type pairs, as add_forbidden_sequence()
    /// recorded them: no dedup, no range check.
    [[nodiscard]] auto const& forbidden_sequences() const noexcept { return forbidden_sequences_; }

    /// Preferences in declaration order: no dedup, no range check.
    [[nodiscard]] auto const& preferences() const noexcept { return preferences_; }

    /// Unavailabilities in declaration order: no dedup, no range check.
    [[nodiscard]] auto const& unavailabilities() const noexcept { return unavailabilities_; }

private:
    // -- Shift types & employees ---------------------------------------------
    std::vector<ShiftTypeParams> shift_types_;
    std::vector<EmployeeParams> employees_;

    // -- Planning horizon ----------------------------------------------------
    int horizon_ = 0;

    // -- Demand entries: (shift_type, day) -> DemandParams -------------------
    std::vector<DemandEntry> demands_;

    // -- Hard constraints ----------------------------------------------------
    std::vector<std::pair<int, int>> forbidden_sequences_;

    // -- Preferences ---------------------------------------------------------
    std::vector<PrefEntry> preferences_;

    // -- Unavailabilities ----------------------------------------------------
    std::vector<UnavailEntry> unavailabilities_;
};

}  // namespace coso
