#pragma once

#include "rostering/rostering_data.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <vector>

namespace coso {

/// Cost evaluator for the rostering engine.
///
/// Evaluates demand violations, hard constraint violations and preference
/// costs.  Supports both full evaluation and delta evaluation for move
/// operators.
class RosteringCostEvaluator {
public:
    /// Penalty weights applied to different violation types.
    struct Weights {
        int understaffing = 1000;    ///< Per missing employee.
        int overstaffing = 100;      ///< Per excess employee.
        int hard_violation = 10000;  ///< Per hard constraint violation.
        int unavailability = 10000;  ///< Per unavailability violation.
        int preference = 1;          ///< Multiplied by preference weight.
    };

    explicit RosteringCostEvaluator(RosteringData const& data) : data_(data), weights_() {}

    RosteringCostEvaluator(RosteringData const& data, Weights weights)
        : data_(data), weights_(weights) {}

    // -- Full evaluation -----------------------------------------------------

    /// Compute total cost for the given schedule matrix.
    /// schedule[employee][day] = shift_type (-1 = unassigned/off).
    [[nodiscard]] int evaluate(std::vector<std::vector<int>> const& schedule) const {
        return demand_cost(schedule) + hard_constraint_cost(schedule) + preference_cost(schedule);
    }

    /// Check whether all hard constraints are satisfied.
    [[nodiscard]] bool is_feasible(std::vector<std::vector<int>> const& schedule) const {
        return hard_constraint_cost(schedule) == 0 && unavailability_cost(schedule) == 0;
    }

    // -- Component costs -----------------------------------------------------

    /// Demand violation cost (under/over-staffing).
    [[nodiscard]] int demand_cost(std::vector<std::vector<int>> const& schedule) const {
        int cost = 0;
        int const ne = data_.num_employees();
        int const ns = data_.num_shift_types();
        int const H = data_.horizon;

        for (int s = 0; s < ns; ++s) {
            for (int d = 0; d < H; ++d) {
                auto dem = data_.get_demand(s, d);
                // Count employees assigned to shift s on day d,
                // respecting skill requirements.
                int count = 0;
                for (int e = 0; e < ne; ++e) {
                    if (schedule[e][d] == s) {
                        if (dem.required_skill.empty() || has_skill(e, dem.required_skill)) {
                            ++count;
                        }
                    }
                }
                if (count < dem.min_employees) {
                    cost += (dem.min_employees - count) * weights_.understaffing;
                }
                if (dem.max_employees < INT_MAX && count > dem.max_employees) {
                    cost += (count - dem.max_employees) * weights_.overstaffing;
                }
            }
        }
        return cost;
    }

    /// Hard constraint violation cost.
    [[nodiscard]] int hard_constraint_cost(std::vector<std::vector<int>> const& schedule) const {
        return consecutive_violation_cost(schedule) + forbidden_sequence_cost(schedule) +
               unavailability_cost(schedule);
    }

    /// Consecutive shift violations.
    [[nodiscard]] int consecutive_violation_cost(
        std::vector<std::vector<int>> const& schedule) const {
        int cost = 0;
        int const ne = data_.num_employees();
        int const H = data_.horizon;

        for (int e = 0; e < ne; ++e) {
            int max_consec = data_.employees[e].max_consecutive_days;
            int run = 0;
            for (int d = 0; d < H; ++d) {
                if (schedule[e][d] >= 0) {
                    ++run;
                } else {
                    run = 0;
                }
                if (run > max_consec) {
                    cost += weights_.hard_violation;
                }
            }
        }
        return cost;
    }

    /// Forbidden shift sequence violations.
    [[nodiscard]] int forbidden_sequence_cost(std::vector<std::vector<int>> const& schedule) const {
        int cost = 0;
        int const ne = data_.num_employees();
        int const H = data_.horizon;

        for (auto const& seq : data_.forbidden_sequences) {
            int const len = static_cast<int>(seq.size());
            if (len < 2) {
                continue;
            }
            for (int e = 0; e < ne; ++e) {
                for (int d = 0; d + len <= H; ++d) {
                    bool match = true;
                    for (int k = 0; k < len; ++k) {
                        if (schedule[e][d + k] != seq[k]) {
                            match = false;
                            break;
                        }
                    }
                    if (match) {
                        cost += weights_.hard_violation;
                    }
                }
            }
        }
        return cost;
    }

    /// Unavailability violations.
    [[nodiscard]] int unavailability_cost(std::vector<std::vector<int>> const& schedule) const {
        int cost = 0;
        int const ne = data_.num_employees();
        int const H = data_.horizon;

        for (int e = 0; e < ne; ++e) {
            for (int d = 0; d < H; ++d) {
                if (schedule[e][d] >= 0 && data_.is_unavailable(e, d)) {
                    cost += weights_.unavailability;
                }
            }
        }
        return cost;
    }

    /// Preference (soft) cost.  Negative weight = penalty for assignment,
    /// positive weight = reward (negative cost contribution).
    [[nodiscard]] int preference_cost(std::vector<std::vector<int>> const& schedule) const {
        int cost = 0;
        for (auto const& p : data_.preferences) {
            if (schedule[p.employee][p.day] == p.shift_type) {
                cost -= p.weight * weights_.preference;
            }
        }
        return cost;
    }

    // -- Delta evaluation ----------------------------------------------------

    /// Cost delta for assigning shift_type to employee on day (was -1).
    [[nodiscard]] int delta_assign(std::vector<std::vector<int>> const& schedule, int employee,
                                   int day, int shift_type) const {
        // Compute cost with and without the assignment.
        auto copy = schedule;
        int before = evaluate(copy);
        copy[employee][day] = shift_type;
        int after = evaluate(copy);
        return after - before;
    }

    /// Cost delta for unassigning employee on day (shift_type -> -1).
    [[nodiscard]] int delta_unassign(std::vector<std::vector<int>> const& schedule, int employee,
                                     int day) const {
        auto copy = schedule;
        int before = evaluate(copy);
        copy[employee][day] = -1;
        int after = evaluate(copy);
        return after - before;
    }

    /// Cost delta for swapping assignments of two employees on a day.
    [[nodiscard]] int delta_swap(std::vector<std::vector<int>> const& schedule, int emp1, int emp2,
                                 int day) const {
        auto copy = schedule;
        int before = evaluate(copy);
        std::swap(copy[emp1][day], copy[emp2][day]);
        int after = evaluate(copy);
        return after - before;
    }

    // -- Accessors -----------------------------------------------------------

    [[nodiscard]] RosteringData const& data() const noexcept { return data_; }
    [[nodiscard]] Weights const& weights() const noexcept { return weights_; }

private:
    [[nodiscard]] bool has_skill(int employee, std::string const& skill) const {
        auto const& skills = data_.employees[employee].skills;
        return std::find(skills.begin(), skills.end(), skill) != skills.end();
    }

    RosteringData const& data_;
    Weights weights_;
};

}  // namespace coso
