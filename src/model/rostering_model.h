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
    int duration_hours = 0;
};

/// Parameters for an employee.
struct EmployeeParams {
    std::string name;
    std::vector<std::string> skills;
    int max_consecutive_days = 5;
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
/// with it, is specified in docs/models.md.
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
