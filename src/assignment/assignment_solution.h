#pragma once

#include "assignment/assignment_data.h"
#include "assignment/cost_evaluator.h"

#include <algorithm>
#include <cassert>
#include <vector>

namespace coso {

/// Solution representation for the assignment (nurse rostering) engine.
///
/// Stores an employee x day matrix where each cell holds a shift type id
/// (-1 = unassigned/off).  Provides assignment, unassignment, and swap
/// operations with delta cost evaluation.
class AssignmentSolution {
public:
    /// Construct an empty solution (all cells = -1).
    explicit AssignmentSolution(AssignmentData const& data,
                                AssignmentCostEvaluator const& evaluator)
        : data_(data)
        , evaluator_(evaluator)
        , schedule_(data.num_employees(),
                    std::vector<int>(data.horizon, -1))
        , cost_(evaluator.evaluate(schedule_))
    {
    }

    // -- Accessors -----------------------------------------------------------

    /// Number of employees.
    [[nodiscard]] int num_employees() const noexcept
    {
        return data_.num_employees();
    }

    /// Planning horizon (number of days).
    [[nodiscard]] int horizon() const noexcept { return data_.horizon; }

    /// Get the shift type for an employee on a day (-1 = unassigned).
    [[nodiscard]] int get(int employee, int day) const
    {
        return schedule_[employee][day];
    }

    /// Read-only access to the full schedule matrix.
    [[nodiscard]] std::vector<std::vector<int>> const& schedule() const noexcept
    {
        return schedule_;
    }

    /// Current total cost (cached).
    [[nodiscard]] int cost() const noexcept { return cost_; }

    /// Check feasibility using the cost evaluator.
    [[nodiscard]] bool is_feasible() const
    {
        return evaluator_.is_feasible(schedule_);
    }

    // -- Modifiers -----------------------------------------------------------

    /// Assign a shift type to an employee on a day.
    /// Returns the cost delta.
    int assign(int employee, int day, int shift_type)
    {
        assert(employee >= 0 && employee < num_employees());
        assert(day >= 0 && day < horizon());
        assert(shift_type >= 0 && shift_type < data_.num_shift_types());

        int delta = evaluator_.delta_assign(schedule_, employee, day, shift_type);
        schedule_[employee][day] = shift_type;
        cost_ += delta;
        return delta;
    }

    /// Unassign an employee from a day (set to -1).
    /// Returns the cost delta.
    int unassign(int employee, int day)
    {
        assert(employee >= 0 && employee < num_employees());
        assert(day >= 0 && day < horizon());

        int delta = evaluator_.delta_unassign(schedule_, employee, day);
        schedule_[employee][day] = -1;
        cost_ += delta;
        return delta;
    }

    /// Swap the assignments of two employees on a given day.
    /// Returns the cost delta.
    int swap(int emp1, int emp2, int day)
    {
        assert(emp1 >= 0 && emp1 < num_employees());
        assert(emp2 >= 0 && emp2 < num_employees());
        assert(day >= 0 && day < horizon());

        int delta = evaluator_.delta_swap(schedule_, emp1, emp2, day);
        std::swap(schedule_[emp1][day], schedule_[emp2][day]);
        cost_ += delta;
        return delta;
    }

    /// Recompute cost from scratch (useful after bulk modifications).
    void recompute_cost()
    {
        cost_ = evaluator_.evaluate(schedule_);
    }

    // -- Evaluation helpers --------------------------------------------------

    /// Demand cost component.
    [[nodiscard]] int demand_cost() const
    {
        return evaluator_.demand_cost(schedule_);
    }

    /// Hard constraint cost component.
    [[nodiscard]] int hard_constraint_cost() const
    {
        return evaluator_.hard_constraint_cost(schedule_);
    }

    /// Preference cost component.
    [[nodiscard]] int preference_cost() const
    {
        return evaluator_.preference_cost(schedule_);
    }

private:
    AssignmentData const& data_;
    AssignmentCostEvaluator const& evaluator_;
    std::vector<std::vector<int>> schedule_;
    int cost_;
};

} // namespace coso
