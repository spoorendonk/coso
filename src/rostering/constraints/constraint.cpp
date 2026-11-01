#include "rostering/constraints/constraint.h"

#include "rostering/cover.h"

#include <algorithm>
#include <climits>
#include <vector>

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

int DemandConstraint::cell_cost(RosteringData const& data, int shift_type, int day,
                                std::vector<int> const& workers) const {
    auto const demands = data.get_demands(shift_type, day);
    if (demands.empty()) {
        return 0;
    }
    // Workers are credited as rostering/cover.h describes, at this
    // constraint's own penalties rather than the demands' declared weights.
    auto const counts = credit_cover(data, demands, workers, [&](int j, int k) {
        auto const& dem = demands[j];
        if (k <= dem.min_employees) {
            return -under_penalty_;
        }
        return k <= dem.max_employees ? 0 : over_penalty_;
    });
    int cost = 0;
    for (size_t j = 0; j < demands.size(); ++j) {
        auto [under, over] = cover_units(demands[j], counts[j]);
        cost += under * under_penalty_ + over * over_penalty_;
    }
    return cost;
}

int DemandConstraint::evaluate(RosteringData const& data,
                               std::vector<std::vector<int>> const& schedule) const {
    int cost = 0;
    int const ns = data.num_shift_types();
    int const H = data.horizon;

    for (int s = 0; s < ns; ++s) {
        for (int d = 0; d < H; ++d) {
            cost += cell_cost(data, s, d, cell_workers(schedule, s, d));
        }
    }
    return cost;
}

int DemandConstraint::evaluate_delta(RosteringData const& data,
                                     std::vector<std::vector<int>> const& schedule,
                                     RosteringMove const& move) const {
    int const d = move.day;
    int delta = 0;

    // The old shift's cell loses the employee; the new shift's cell gains one.
    if (move.old_shift >= 0) {
        auto workers = cell_workers(schedule, move.old_shift, d);
        delta -= cell_cost(data, move.old_shift, d, workers);
        std::erase(workers, move.employee);
        delta += cell_cost(data, move.old_shift, d, workers);
    }
    if (move.new_shift >= 0) {
        auto workers = cell_workers(schedule, move.new_shift, d);
        delta -= cell_cost(data, move.new_shift, d, workers);
        workers.push_back(move.employee);
        delta += cell_cost(data, move.new_shift, d, workers);
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
