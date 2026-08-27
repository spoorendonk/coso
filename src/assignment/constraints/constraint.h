#pragma once

#include "assignment/assignment_data.h"

#include <algorithm>
#include <climits>
#include <memory>
#include <string>
#include <vector>

namespace coso {

/// Move descriptor for delta evaluation in the assignment engine.
///
/// Represents a single cell change: assigning employee `employee` on day `day`
/// from `old_shift` to `new_shift`.  A shift value of -1 means unassigned/off.
struct AssignmentMove {
    int employee = -1;
    int day = -1;
    int old_shift = -1;  ///< Previous shift type (-1 = off).
    int new_shift = -1;  ///< New shift type (-1 = off).
};

/// Abstract base class for assignment constraints.
///
/// Each constraint can be evaluated in two ways:
///   1. Full evaluation: scan the entire schedule and return the total cost.
///   2. Delta evaluation: given a single-cell move, return the cost change.
///
/// By convention, cost >= 0 and a cost of 0 means the constraint is fully
/// satisfied.  Hard constraints use large penalty weights; soft constraints
/// use smaller ones.
class Constraint {
public:
    virtual ~Constraint() = default;

    /// Full evaluation: total cost contribution of this constraint.
    [[nodiscard]] virtual int evaluate(AssignmentData const& data,
                                       std::vector<std::vector<int>> const& schedule) const = 0;

    /// Incremental delta evaluation.
    ///
    /// Returns the change in cost if `move` is applied to `schedule`.
    /// The default implementation recomputes via full evaluation (correct but
    /// slow); concrete constraints should override for O(1) or O(horizon).
    [[nodiscard]] virtual int evaluate_delta(AssignmentData const& data,
                                             std::vector<std::vector<int>> const& schedule,
                                             AssignmentMove const& move) const;

    /// Human-readable name for debugging / reporting.
    [[nodiscard]] virtual std::string name() const = 0;
};

// --------------------------------------------------------------------------- //
//  Concrete constraints                                                        //
// --------------------------------------------------------------------------- //

/// Penalizes exceeding the maximum number of consecutive working days.
///
/// Uses the per-employee limit (Employee::max_consecutive_days) clamped by the
/// global limit (AssignmentData::max_consecutive_shifts).
class MaxConsecutiveConstraint final : public Constraint {
public:
    explicit MaxConsecutiveConstraint(int penalty = 10000) : penalty_(penalty) {}

    [[nodiscard]] int evaluate(AssignmentData const& data,
                               std::vector<std::vector<int>> const& schedule) const override;

    [[nodiscard]] int evaluate_delta(AssignmentData const& data,
                                     std::vector<std::vector<int>> const& schedule,
                                     AssignmentMove const& move) const override;

    [[nodiscard]] std::string name() const override { return "MaxConsecutive"; }

private:
    int penalty_;

    /// Count violations for a single employee row.
    [[nodiscard]] int employee_cost(int max_consec, std::vector<int> const& row, int horizon) const;
};

/// Penalizes insufficient rest hours between consecutive shifts.
///
/// Rest is computed as (24 - end_hour_of_prev_shift) + start_hour_of_next_shift.
class MinRestConstraint final : public Constraint {
public:
    explicit MinRestConstraint(int penalty = 10000) : penalty_(penalty) {}

    [[nodiscard]] int evaluate(AssignmentData const& data,
                               std::vector<std::vector<int>> const& schedule) const override;

    [[nodiscard]] int evaluate_delta(AssignmentData const& data,
                                     std::vector<std::vector<int>> const& schedule,
                                     AssignmentMove const& move) const override;

    [[nodiscard]] std::string name() const override { return "MinRest"; }

private:
    int penalty_;

    /// Check rest between two shifts.  Returns penalty if violated, 0 otherwise.
    [[nodiscard]] int check_rest(AssignmentData const& data, int min_rest, int s1, int s2) const;
};

/// Penalizes under-staffing and over-staffing relative to demand.
class DemandConstraint final : public Constraint {
public:
    explicit DemandConstraint(int understaffing_penalty = 1000, int overstaffing_penalty = 100)
        : under_penalty_(understaffing_penalty), over_penalty_(overstaffing_penalty) {}

    [[nodiscard]] int evaluate(AssignmentData const& data,
                               std::vector<std::vector<int>> const& schedule) const override;

    [[nodiscard]] int evaluate_delta(AssignmentData const& data,
                                     std::vector<std::vector<int>> const& schedule,
                                     AssignmentMove const& move) const override;

    [[nodiscard]] std::string name() const override { return "Demand"; }

private:
    int under_penalty_;
    int over_penalty_;

    /// Count employees with matching skills assigned to (shift, day).
    [[nodiscard]] int count_assigned(AssignmentData const& data,
                                     std::vector<std::vector<int>> const& schedule, int shift_type,
                                     int day) const;
};

/// Penalizes forbidden shift sequences.
///
/// A forbidden sequence is a list of shift-type ids that must not appear on
/// consecutive days for any employee.
class ForbiddenSequenceConstraint final : public Constraint {
public:
    explicit ForbiddenSequenceConstraint(int penalty = 10000) : penalty_(penalty) {}

    [[nodiscard]] int evaluate(AssignmentData const& data,
                               std::vector<std::vector<int>> const& schedule) const override;

    [[nodiscard]] int evaluate_delta(AssignmentData const& data,
                                     std::vector<std::vector<int>> const& schedule,
                                     AssignmentMove const& move) const override;

    [[nodiscard]] std::string name() const override { return "ForbiddenSequence"; }

private:
    int penalty_;

    /// Count forbidden-sequence violations for a single employee.
    [[nodiscard]] int employee_cost(AssignmentData const& data, std::vector<int> const& row,
                                    int horizon) const;
};

/// Soft preference constraint.
///
/// When an employee is assigned to their preferred shift on a preferred day,
/// a negative cost (reward) is applied.  When assigned to a dispreferred shift,
/// a positive cost (penalty) results.  The weight in each Preference entry is
/// multiplied by a global weight factor.
class PreferenceConstraint final : public Constraint {
public:
    explicit PreferenceConstraint(int weight = 1) : weight_(weight) {}

    [[nodiscard]] int evaluate(AssignmentData const& data,
                               std::vector<std::vector<int>> const& schedule) const override;

    [[nodiscard]] int evaluate_delta(AssignmentData const& data,
                                     std::vector<std::vector<int>> const& schedule,
                                     AssignmentMove const& move) const override;

    [[nodiscard]] std::string name() const override { return "Preference"; }

private:
    int weight_;
};

// --------------------------------------------------------------------------- //
//  Composite constraint evaluator                                              //
// --------------------------------------------------------------------------- //

/// Holds a set of constraints and evaluates them together.
///
/// Provides the same evaluate / evaluate_delta interface as individual
/// constraints, aggregating results across all registered constraints.
class ConstraintEvaluator {
public:
    ConstraintEvaluator() = default;

    /// Add a constraint.  Takes ownership via unique_ptr.
    void add(std::unique_ptr<Constraint> c) { constraints_.push_back(std::move(c)); }

    /// Total cost across all constraints.
    [[nodiscard]] int evaluate(AssignmentData const& data,
                               std::vector<std::vector<int>> const& schedule) const {
        int total = 0;
        for (auto const& c : constraints_) {
            total += c->evaluate(data, schedule);
        }
        return total;
    }

    /// Total delta across all constraints.
    [[nodiscard]] int evaluate_delta(AssignmentData const& data,
                                     std::vector<std::vector<int>> const& schedule,
                                     AssignmentMove const& move) const {
        int total = 0;
        for (auto const& c : constraints_) {
            total += c->evaluate_delta(data, schedule, move);
        }
        return total;
    }

    /// Number of registered constraints.
    [[nodiscard]] int size() const noexcept { return static_cast<int>(constraints_.size()); }

    /// Per-constraint cost breakdown (for reporting).
    [[nodiscard]] std::vector<std::pair<std::string, int>> breakdown(
        AssignmentData const& data, std::vector<std::vector<int>> const& schedule) const {
        std::vector<std::pair<std::string, int>> result;
        result.reserve(constraints_.size());
        for (auto const& c : constraints_) {
            result.emplace_back(c->name(), c->evaluate(data, schedule));
        }
        return result;
    }

private:
    std::vector<std::unique_ptr<Constraint>> constraints_;
};

}  // namespace coso
