#pragma once

#include "types.h"

#include <climits>
#include <stdexcept>
#include <string>
#include <vector>

namespace coso {

/// Parameters for a shift type.
struct ShiftTypeParams {
    std::string name;
    int start_hour = 0;
    int end_hour = 8;
    int duration_hours = 0;  ///< 0 = computed from start/end
};

/// Parameters for an employee.
struct EmployeeParams {
    std::string name;
    std::vector<std::string> skills;
    int max_hours_per_week = 40;
    int max_consecutive_days = 5;
    int min_rest_hours = 11;
};

/// Demand parameters for a shift on a given day.
struct DemandParams {
    int min_employees = 0;
    int max_employees = INT_MAX;
    std::string required_skill;  ///< empty = no skill requirement
};

/// Assignment model: declare employees, shifts, demands, constraints, then solve.
///
/// Supports nurse rostering, timetabling, and related assignment problems.
class AssignmentModel {
public:
    /// One demand entry: (shift_type, day) -> DemandParams.
    struct DemandEntry {
        int shift_type;
        int day;
        DemandParams params;
    };

    /// One all-days demand entry: shift_type -> DemandParams.
    struct DemandAllEntry {
        int shift_type;
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

    /// Add a demand for a shift type on all days of the horizon.
    void add_demand(int shift_type, DemandParams p);

    // -- Constraints (hard) --------------------------------------------------

    /// Set the maximum number of consecutive shifts for all employees.
    void set_max_consecutive_shifts(int n);

    /// Set the minimum rest period between shifts (in hours).
    void set_min_rest_between_shifts(int hours);

    /// Forbid a specific sequence of shift types.
    void add_forbidden_sequence(const std::vector<int>& shift_types);

    // -- Preferences (soft) --------------------------------------------------

    /// Add a preference weight for an employee on a specific day/shift.
    void add_preference(int employee, int day, int shift_type, int weight);

    /// Mark an employee as unavailable on a specific day.
    void add_unavailability(int employee, int day);

    // -- Warm start / replanning ---------------------------------------------

    /// Provide a published schedule: employee x day -> shift type.
    void set_published_schedule(const std::vector<std::vector<int>>& schedule);

    /// Set the penalty cost per deviation from the published schedule.
    void set_change_penalty(int penalty);

    // -- Solve ---------------------------------------------------------------

    /// Solve the assignment problem within the given time limit.
    Result solve(TimeLimit tl);

    // -- Accessors -----------------------------------------------------------

    [[nodiscard]] int num_shift_types() const noexcept {
        return static_cast<int>(shift_types_.size());
    }
    [[nodiscard]] ShiftTypeParams const& shift_type(int s) const {
        if (s < 0 || static_cast<size_t>(s) >= shift_types_.size()) {
            throw std::out_of_range("AssignmentModel::shift_type: invalid index");
        }
        return shift_types_[s];
    }

    [[nodiscard]] int num_employees() const noexcept { return static_cast<int>(employees_.size()); }
    [[nodiscard]] EmployeeParams const& employee(int e) const {
        if (e < 0 || static_cast<size_t>(e) >= employees_.size()) {
            throw std::out_of_range("AssignmentModel::employee: invalid index");
        }
        return employees_[e];
    }

    [[nodiscard]] int horizon() const noexcept { return horizon_; }

    /// Per-day demands from add_demand(shift_type, day, p), in declaration
    /// order: no dedup, no range check, and never merged with demands_all().
    [[nodiscard]] auto const& demands() const noexcept { return demands_; }

    /// All-days demands from add_demand(shift_type, p), in declaration order:
    /// stored apart from demands(), unexpanded over the horizon.
    [[nodiscard]] auto const& demands_all() const noexcept { return demands_all_; }

    [[nodiscard]] int max_consecutive_shifts() const noexcept { return max_consecutive_shifts_; }
    [[nodiscard]] int min_rest_between_shifts() const noexcept { return min_rest_between_shifts_; }

    /// Forbidden shift-type sequences, as add_forbidden_sequence() recorded
    /// them: no dedup, no range check.
    [[nodiscard]] auto const& forbidden_sequences() const noexcept { return forbidden_sequences_; }

    /// Preferences in declaration order: no dedup, no range check.
    [[nodiscard]] auto const& preferences() const noexcept { return preferences_; }

    /// Unavailabilities in declaration order: no dedup, no range check.
    [[nodiscard]] auto const& unavailabilities() const noexcept { return unavailabilities_; }

    /// The reference schedule from set_published_schedule(), employee x day.
    [[nodiscard]] auto const& published_schedule() const noexcept { return published_schedule_; }

    [[nodiscard]] int change_penalty() const noexcept { return change_penalty_; }

private:
    // -- Shift types & employees ---------------------------------------------
    std::vector<ShiftTypeParams> shift_types_;
    std::vector<EmployeeParams> employees_;

    // -- Planning horizon ----------------------------------------------------
    int horizon_ = 0;

    // -- Demand entries: (shift_type, day) -> DemandParams -------------------
    std::vector<DemandEntry> demands_;

    // -- Demand for all days: shift_type -> DemandParams ---------------------
    std::vector<DemandAllEntry> demands_all_;

    // -- Hard constraints ----------------------------------------------------
    int max_consecutive_shifts_ = INT_MAX;
    int min_rest_between_shifts_ = 0;
    std::vector<std::vector<int>> forbidden_sequences_;

    // -- Preferences ---------------------------------------------------------
    std::vector<PrefEntry> preferences_;

    // -- Unavailabilities ----------------------------------------------------
    std::vector<UnavailEntry> unavailabilities_;

    // -- Replanning ----------------------------------------------------------
    std::vector<std::vector<int>> published_schedule_;
    int change_penalty_ = 0;
};

}  // namespace coso
