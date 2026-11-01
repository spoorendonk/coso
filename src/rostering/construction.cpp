#include "rostering/construction.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <numeric>
#include <vector>

namespace coso {

namespace {

/// Minutes employee e works under `schedule`.
int64_t worked_minutes(RosteringData const& data, std::vector<std::vector<int>> const& schedule,
                       int e) {
    int64_t total = 0;
    for (int s : schedule[e]) {
        if (s >= 0) {
            total += data.shift_types[s].duration_minutes;
        }
    }
    return total;
}

/// Check whether assigning employee e to shift s on day d would violate
/// any hard constraint: unavailability, consecutive days, forbidden
/// sequences, max_weekends and max_total_minutes.  min_total_minutes is a
/// lower bound no single assignment can break; top_up_min_minutes owns it.
bool is_feasible_assignment(RosteringData const& data,
                            std::vector<std::vector<int>> const& schedule, int e, int d, int s) {
    // Unavailability.
    if (data.is_unavailable(e, d)) {
        return false;
    }

    // Already assigned on this day.
    if (schedule[e][d] >= 0) {
        return false;
    }

    // Consecutive days limit.
    int max_consec = data.employees[e].max_consecutive_days;
    // Count the run of consecutive working days ending at d-1.
    int run_before = 0;
    for (int dd = d - 1; dd >= 0 && schedule[e][dd] >= 0; --dd) {
        ++run_before;
    }
    // Count the run of consecutive working days starting at d+1.
    int run_after = 0;
    for (int dd = d + 1; dd < data.horizon && schedule[e][dd] >= 0; ++dd) {
        ++run_after;
    }
    if (run_before + 1 + run_after > max_consec) {
        return false;
    }

    // Forbidden sequences: check if adding shift s on day d creates one.
    for (auto const& seq : data.forbidden_sequences) {
        int len = static_cast<int>(seq.size());
        if (len < 2) {
            continue;
        }
        // For each position k in the sequence where day d could contribute,
        // check if the full sequence would be matched.
        for (int k = 0; k < len; ++k) {
            int start_day = d - k;
            if (start_day < 0 || start_day + len > data.horizon) {
                continue;
            }
            bool match = true;
            for (int j = 0; j < len; ++j) {
                int dd = start_day + j;
                int shift_on_dd = (dd == d) ? s : schedule[e][dd];
                if (shift_on_dd != seq[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return false;
            }
        }
    }

    // Distinct weekends: a shift on a weekend day the employee does not
    // already work must not take them past max_weekends.
    int const max_weekends = data.employees[e].max_weekends;
    if (RosteringData::is_weekend_day(d) && max_weekends < data.horizon) {
        int const other = (d % 7 == 5) ? d + 1 : d - 1;
        bool const weekend_already_worked = other < data.horizon && schedule[e][other] >= 0;
        if (!weekend_already_worked) {
            int worked = 0;
            for (int sat = 5; sat < data.horizon; sat += 7) {
                if (schedule[e][sat] >= 0 ||
                    (sat + 1 < data.horizon && schedule[e][sat + 1] >= 0)) {
                    ++worked;
                }
            }
            if (worked >= max_weekends) {
                return false;
            }
        }
    }

    // Maximum working time over the horizon.
    if (data.employees[e].max_total_minutes != INT_MAX &&
        worked_minutes(data, schedule, e) + data.shift_types[s].duration_minutes >
            static_cast<int64_t>(data.employees[e].max_total_minutes)) {
        return false;
    }

    return true;
}

/// Check whether employee has a given skill (empty skill = always true).
bool employee_has_skill(RosteringData const& data, int e, std::string const& skill) {
    if (skill.empty()) {
        return true;
    }
    auto const& skills = data.employees[e].skills;
    return std::find(skills.begin(), skills.end(), skill) != skills.end();
}

/// Bring every employee up to min_total_minutes, which the demand-driven
/// passes never aim at.  Each step adds the feasible (day, shift) whose cover
/// effect is cheapest: filling a shortfall first, then an assignment within
/// max_employees, then a soft over-cover; never a hard over-cover.  An
/// employee the step cannot bring up to the minimum is left short, so the
/// evaluator reports the roster infeasible rather than the construction
/// hiding it.
void top_up_min_minutes(RosteringData const& data, RosteringSolution& sol) {
    int const ns = data.num_shift_types();
    int const H = data.horizon;
    int const ne = data.num_employees();

    for (int e = 0; e < ne; ++e) {
        int64_t const min_minutes = data.employees[e].min_total_minutes;
        while (worked_minutes(data, sol.schedule(), e) < min_minutes) {
            int best_d = -1;
            int best_s = -1;
            int64_t best_score = 0;
            for (int d = 0; d < H; ++d) {
                for (int s = 0; s < ns; ++s) {
                    if (!is_feasible_assignment(data, sol.schedule(), e, d, s)) {
                        continue;
                    }
                    auto const dem = data.get_demand(s, d);
                    int64_t score = 0;
                    if (employee_has_skill(data, e, dem.required_skill)) {
                        int count = 0;
                        for (int x = 0; x < ne; ++x) {
                            if (sol.get(x, d) == s &&
                                employee_has_skill(data, x, dem.required_skill)) {
                                ++count;
                            }
                        }
                        if (count < dem.min_employees) {
                            score = dem.hard ? INT_MIN : -dem.under_weight;
                        } else if (dem.max_employees != INT_MAX && count >= dem.max_employees) {
                            if (dem.hard) {
                                continue;
                            }
                            score = dem.over_weight;
                        }
                    }
                    if (best_d < 0 || score < best_score) {
                        best_d = d;
                        best_s = s;
                        best_score = score;
                    }
                }
            }
            if (best_d < 0) {
                break;  // Cannot reach the minimum; the roster stays infeasible.
            }
            sol.assign(e, best_d, best_s);
        }
    }
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
//  First-Fit-Decreasing
// ---------------------------------------------------------------------------

RosteringSolution construct_ffd(RosteringData const& data,
                                RosteringCostEvaluator const& evaluator) {
    RosteringSolution sol(data, evaluator);

    // Build list of (shift_type, day, min_employees) demand entries.
    struct DemandEntry {
        int shift_type;
        int day;
        int min_employees;
        std::string required_skill;
    };

    std::vector<DemandEntry> demands;
    int const ns = data.num_shift_types();
    int const H = data.horizon;

    for (int s = 0; s < ns; ++s) {
        for (int d = 0; d < H; ++d) {
            auto dem = data.get_demand(s, d);
            if (dem.min_employees > 0) {
                demands.push_back({s, d, dem.min_employees, dem.required_skill});
            }
        }
    }

    // Sort by required employees descending (FFD ordering).
    std::sort(demands.begin(), demands.end(), [](DemandEntry const& a, DemandEntry const& b) {
        return a.min_employees > b.min_employees;
    });

    // For each demand entry, assign employees.
    for (auto const& dem : demands) {
        // Count how many are already assigned to this (shift, day).
        int assigned = 0;
        for (int e = 0; e < data.num_employees(); ++e) {
            if (sol.get(e, dem.day) == dem.shift_type &&
                employee_has_skill(data, e, dem.required_skill)) {
                ++assigned;
            }
        }

        // Sort employees by assignment cost delta (cheapest first).
        std::vector<int> candidates(data.num_employees());
        std::iota(candidates.begin(), candidates.end(), 0);

        // Filter and sort: prefer employees that are feasible, then by cost.
        std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
            // Prefer employees with fewer total assigned shifts
            // (load-balancing heuristic).
            int load_a = 0, load_b = 0;
            for (int dd = 0; dd < H; ++dd) {
                if (sol.get(a, dd) >= 0) {
                    ++load_a;
                }
                if (sol.get(b, dd) >= 0) {
                    ++load_b;
                }
            }
            return load_a < load_b;
        });

        while (assigned < dem.min_employees) {
            bool found = false;
            for (int e : candidates) {
                if (!employee_has_skill(data, e, dem.required_skill)) {
                    continue;
                }
                if (!is_feasible_assignment(data, sol.schedule(), e, dem.day, dem.shift_type)) {
                    continue;
                }
                sol.assign(e, dem.day, dem.shift_type);
                ++assigned;
                found = true;
                break;
            }
            if (!found) {
                break;  // No feasible employee available.
            }
        }
    }

    top_up_min_minutes(data, sol);
    return sol;
}

// ---------------------------------------------------------------------------
//  Greedy day-by-day
// ---------------------------------------------------------------------------

RosteringSolution construct_greedy(RosteringData const& data,
                                   RosteringCostEvaluator const& evaluator) {
    RosteringSolution sol(data, evaluator);

    int const ns = data.num_shift_types();
    int const H = data.horizon;
    int const ne = data.num_employees();

    // Iterate day by day, shift by shift.
    for (int d = 0; d < H; ++d) {
        for (int s = 0; s < ns; ++s) {
            auto dem = data.get_demand(s, d);
            if (dem.min_employees <= 0) {
                continue;
            }

            // Count currently assigned employees.
            int assigned = 0;
            for (int e = 0; e < ne; ++e) {
                if (sol.get(e, d) == s && employee_has_skill(data, e, dem.required_skill)) {
                    ++assigned;
                }
            }

            // Sort employees by cost delta for this assignment.
            std::vector<int> candidates(ne);
            std::iota(candidates.begin(), candidates.end(), 0);
            std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
                // Sort by: fewest assigned days so far (load balance).
                int load_a = 0, load_b = 0;
                for (int dd = 0; dd < H; ++dd) {
                    if (sol.get(a, dd) >= 0) {
                        ++load_a;
                    }
                    if (sol.get(b, dd) >= 0) {
                        ++load_b;
                    }
                }
                return load_a < load_b;
            });

            // Assign greedily until demand is met.
            while (assigned < dem.min_employees) {
                bool found = false;
                for (int e : candidates) {
                    if (!employee_has_skill(data, e, dem.required_skill)) {
                        continue;
                    }
                    if (!is_feasible_assignment(data, sol.schedule(), e, d, s)) {
                        continue;
                    }
                    sol.assign(e, d, s);
                    ++assigned;
                    found = true;
                    break;
                }
                if (!found) {
                    break;
                }
            }
        }
    }

    top_up_min_minutes(data, sol);
    return sol;
}

}  // namespace coso
