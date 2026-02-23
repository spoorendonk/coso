#pragma once

#include "types.h"

#include <climits>
#include <string>
#include <vector>

namespace coso {

/// Parameters for a shift type.
struct ShiftTypeParams {
    std::string name;
    int start_hour     = 0;
    int end_hour       = 8;
    int duration_hours = 0;   ///< 0 = computed from start/end
};

/// Parameters for an employee.
struct EmployeeParams {
    std::string name;
    std::vector<std::string> skills;
    int max_hours_per_week  = 40;
    int max_consecutive_days = 5;
    int min_rest_hours       = 11;
};

/// Demand parameters for a shift on a given day.
struct DemandParams {
    int min_employees       = 0;
    int max_employees       = INT_MAX;
    std::string required_skill;   ///< empty = no skill requirement
};

/// Assignment model: declare employees, shifts, demands, constraints, then solve.
///
/// Supports nurse rostering, timetabling, and related assignment problems.
class AssignmentModel {
public:
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
    void set_published_schedule(
        const std::vector<std::vector<int>>& schedule);

    /// Set the penalty cost per deviation from the published schedule.
    void set_change_penalty(int penalty);

    // -- Solve ---------------------------------------------------------------

    /// Solve the assignment problem within the given time limit.
    Result solve(TimeLimit tl);
};

} // namespace coso
