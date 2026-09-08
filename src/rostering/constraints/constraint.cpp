#include "rostering/constraints/constraint.h"

#include <algorithm>
#include <climits>

namespace coso {

// --------------------------------------------------------------------------- //
//  Constraint (base) — default delta via full recompute                        //
// --------------------------------------------------------------------------- //

int Constraint::evaluate_delta(RosteringData const& data,
                               std::vector<std::vector<int>> const& schedule,
                               RosteringMove const& move) const {
    int before = evaluate(data, schedule);
    auto copy = schedule;
    copy[move.employee][move.day] = move.new_shift;
    int after = evaluate(data, copy);
    return after - before;
}

// --------------------------------------------------------------------------- //
//  MaxConsecutiveConstraint                                                     //
// --------------------------------------------------------------------------- //

int MaxConsecutiveConstraint::employee_cost(int max_consec, std::vector<int> const& row,
                                            int horizon) const {
    int cost = 0;
    int run = 0;
    for (int d = 0; d < horizon; ++d) {
        if (row[d] >= 0) {
            ++run;
        } else {
            run = 0;
        }
        if (run > max_consec) {
            cost += penalty_;
        }
    }
    return cost;
}

int MaxConsecutiveConstraint::evaluate(RosteringData const& data,
                                       std::vector<std::vector<int>> const& schedule) const {
    int cost = 0;
    int const ne = data.num_employees();
    int const H = data.horizon;

    for (int e = 0; e < ne; ++e) {
        int max_consec = data.employees[e].max_consecutive_days;
        cost += employee_cost(max_consec, schedule[e], H);
    }
    return cost;
}

int MaxConsecutiveConstraint::evaluate_delta(RosteringData const& data,
                                             std::vector<std::vector<int>> const& schedule,
                                             RosteringMove const& move) const {
    // Only the affected employee's row changes.
    int e = move.employee;
    int max_consec = data.employees[e].max_consecutive_days;
    int H = data.horizon;

    int before = employee_cost(max_consec, schedule[e], H);

    // Build modified row.
    auto row = schedule[e];
    row[move.day] = move.new_shift;
    int after = employee_cost(max_consec, row, H);

    return after - before;
}

// --------------------------------------------------------------------------- //
//  DemandConstraint                                                             //
// --------------------------------------------------------------------------- //

int DemandConstraint::count_assigned(RosteringData const& data,
                                     std::vector<std::vector<int>> const& schedule, int shift_type,
                                     int day) const {
    auto dem = data.get_demand(shift_type, day);
    int count = 0;
    int const ne = data.num_employees();
    for (int e = 0; e < ne; ++e) {
        if (schedule[e][day] == shift_type) {
            if (dem.required_skill.empty() ||
                std::find(data.employees[e].skills.begin(), data.employees[e].skills.end(),
                          dem.required_skill) != data.employees[e].skills.end()) {
                ++count;
            }
        }
    }
    return count;
}

int DemandConstraint::evaluate(RosteringData const& data,
                               std::vector<std::vector<int>> const& schedule) const {
    int cost = 0;
    int const ns = data.num_shift_types();
    int const H = data.horizon;

    for (int s = 0; s < ns; ++s) {
        for (int d = 0; d < H; ++d) {
            auto dem = data.get_demand(s, d);
            int count = count_assigned(data, schedule, s, d);
            if (count < dem.min_employees) {
                cost += (dem.min_employees - count) * under_penalty_;
            }
            if (dem.max_employees < INT_MAX && count > dem.max_employees) {
                cost += (count - dem.max_employees) * over_penalty_;
            }
        }
    }
    return cost;
}

int DemandConstraint::evaluate_delta(RosteringData const& data,
                                     std::vector<std::vector<int>> const& schedule,
                                     RosteringMove const& move) const {
    int d = move.day;
    int delta = 0;

    // Helper to compute demand cost for a single (shift, day) cell.
    auto cell_cost = [&](int shift_type, int count) -> int {
        auto dem = data.get_demand(shift_type, d);
        int c = 0;
        if (count < dem.min_employees) {
            c += (dem.min_employees - count) * under_penalty_;
        }
        if (dem.max_employees < INT_MAX && count > dem.max_employees) {
            c += (count - dem.max_employees) * over_penalty_;
        }
        return c;
    };

    // Check if the employee has the skill for the shift.
    auto has_skill = [&](int emp, int shift_type) -> bool {
        auto dem = data.get_demand(shift_type, d);
        if (dem.required_skill.empty()) {
            return true;
        }
        auto const& skills = data.employees[emp].skills;
        return std::find(skills.begin(), skills.end(), dem.required_skill) != skills.end();
    };

    // The old shift loses one employee; the new shift gains one.
    if (move.old_shift >= 0 && has_skill(move.employee, move.old_shift)) {
        int count = count_assigned(data, schedule, move.old_shift, d);
        delta -= cell_cost(move.old_shift, count);
        delta += cell_cost(move.old_shift, count - 1);
    }
    if (move.new_shift >= 0 && has_skill(move.employee, move.new_shift)) {
        int count = count_assigned(data, schedule, move.new_shift, d);
        delta -= cell_cost(move.new_shift, count);
        delta += cell_cost(move.new_shift, count + 1);
    }

    return delta;
}

// --------------------------------------------------------------------------- //
//  ForbiddenSequenceConstraint                                                  //
// --------------------------------------------------------------------------- //

int ForbiddenSequenceConstraint::employee_cost(RosteringData const& data,
                                               std::vector<int> const& row, int horizon) const {
    int cost = 0;
    for (auto const& seq : data.forbidden_sequences) {
        int const len = static_cast<int>(seq.size());
        if (len < 2) {
            continue;
        }
        for (int d = 0; d + len <= horizon; ++d) {
            bool match = true;
            for (int k = 0; k < len; ++k) {
                if (row[d + k] != seq[k]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                cost += penalty_;
            }
        }
    }
    return cost;
}

int ForbiddenSequenceConstraint::evaluate(RosteringData const& data,
                                          std::vector<std::vector<int>> const& schedule) const {
    int cost = 0;
    int const ne = data.num_employees();
    int const H = data.horizon;
    for (int e = 0; e < ne; ++e) {
        cost += employee_cost(data, schedule[e], H);
    }
    return cost;
}

int ForbiddenSequenceConstraint::evaluate_delta(RosteringData const& data,
                                                std::vector<std::vector<int>> const& schedule,
                                                RosteringMove const& move) const {
    // Only the affected employee's row changes.
    int e = move.employee;
    int H = data.horizon;

    int before = employee_cost(data, schedule[e], H);

    auto row = schedule[e];
    row[move.day] = move.new_shift;
    int after = employee_cost(data, row, H);

    return after - before;
}

// --------------------------------------------------------------------------- //
//  PreferenceConstraint                                                         //
// --------------------------------------------------------------------------- //

int PreferenceConstraint::evaluate(RosteringData const& data,
                                   std::vector<std::vector<int>> const& schedule) const {
    int cost = 0;
    for (auto const& p : data.preferences) {
        if (schedule[p.employee][p.day] == p.shift_type) {
            cost -= p.weight * weight_;
        }
    }
    return cost;
}

int PreferenceConstraint::evaluate_delta(RosteringData const& data,
                                         std::vector<std::vector<int>> const& schedule,
                                         RosteringMove const& move) const {
    int delta = 0;
    for (auto const& p : data.preferences) {
        if (p.employee != move.employee || p.day != move.day) {
            continue;
        }
        // Was satisfied before?
        if (p.shift_type == move.old_shift) {
            delta += p.weight * weight_;  // Losing the reward.
        }
        // Will be satisfied after?
        if (p.shift_type == move.new_shift) {
            delta -= p.weight * weight_;  // Gaining the reward.
        }
    }
    return delta;
}

}  // namespace coso
