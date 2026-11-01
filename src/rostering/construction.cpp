#include "rostering/construction.h"

#include "rostering/cover.h"

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

}  // anonymous namespace

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

namespace {

/// A top-up step: assign shift s on day d, at cover cost `score`.
struct TopUpCandidate {
    int d;
    int s;
    int64_t score;
};

/// Every (day, shift) from `from_day` on that a top-up may give employee e:
/// feasible, open to it, adding minutes, and never a hard over-cover.  Each
/// carries the change in its cell's cover cost, with workers credited as
/// rostering/cover.h describes, so a shortfall fill scores lowest, then an
/// assignment within max_employees, then a soft over-cover.  Sorted by score,
/// then day, then shift.
std::vector<TopUpCandidate> top_up_candidates(RosteringData const& data,
                                              RosteringSolution const& sol, int e, int from_day,
                                              CellFilter const& may_assign) {
    std::vector<TopUpCandidate> out;
    for (int d = from_day; d < data.horizon; ++d) {
        if (may_assign && !may_assign(e, d)) {
            continue;
        }
        for (int s = 0; s < data.num_shift_types(); ++s) {
            // A shift adding no minutes cannot close the gap.
            if (data.shift_types[s].duration_minutes <= 0 ||
                !is_feasible_assignment(data, sol.schedule(), e, d, s)) {
                continue;
            }
            auto workers = cell_workers(sol.schedule(), s, d);
            auto const [soft_before, hard_before] = cell_cover_cost(data, s, d, workers);
            workers.push_back(e);
            auto const [soft_after, hard_after] = cell_cover_cost(data, s, d, workers);
            if (hard_after > hard_before) {
                continue;  // Never a hard over-cover.
            }
            out.push_back({d, s,
                           static_cast<int64_t>(soft_after - soft_before) +
                               static_cast<int64_t>(hard_after - hard_before)});
        }
    }
    std::stable_sort(out.begin(), out.end(), [](TopUpCandidate const& a, TopUpCandidate const& b) {
        return a.score < b.score;
    });
    return out;
}

/// Nodes the backtracking top-up may expand per employee before it gives up.
constexpr int kTopUpNodeBudget = 20000;

/// Depth-first search for a set of extra shifts that brings employee e from
/// `worked` up to `min_minutes`, adding them in day order from `from_day`.
/// Every hard bound is_feasible_assignment checks only tightens as shifts are
/// added, so a set passes it in day order if it passes in any order, and
/// day order loses no solution.  On success the shifts stay assigned; on
/// failure every shift it added is undone.
bool top_up_search(RosteringData const& data, RosteringSolution& sol, int e, int from_day,
                   int64_t worked, int64_t min_minutes, int max_duration,
                   CellFilter const& may_assign, int& budget) {
    if (worked >= min_minutes) {
        return true;
    }
    if (--budget < 0) {
        return false;
    }
    // Prune: even a longest shift on every remaining open day falls short.
    int64_t open_days = 0;
    for (int d = from_day; d < data.horizon; ++d) {
        if (sol.get(e, d) < 0 && !data.is_unavailable(e, d) && (!may_assign || may_assign(e, d))) {
            ++open_days;
        }
    }
    if (worked + open_days * max_duration < min_minutes) {
        return false;
    }
    for (auto const& c : top_up_candidates(data, sol, e, from_day, may_assign)) {
        sol.assign(e, c.d, c.s);
        if (top_up_search(data, sol, e, c.d + 1, worked + data.shift_types[c.s].duration_minutes,
                          min_minutes, max_duration, may_assign, budget)) {
            return true;
        }
        sol.unassign(e, c.d);
        if (budget < 0) {
            return false;
        }
    }
    return false;
}

}  // anonymous namespace

void top_up_min_minutes(RosteringData const& data, RosteringSolution& sol,
                        CellFilter const& may_assign) {
    int max_duration = 0;
    for (auto const& st : data.shift_types) {
        max_duration = std::max(max_duration, st.duration_minutes);
    }

    for (int e = 0; e < data.num_employees(); ++e) {
        int64_t const min_minutes = data.employees[e].min_total_minutes;
        int64_t const start = worked_minutes(data, sol.schedule(), e);
        if (start >= min_minutes) {
            continue;
        }

        // Greedy first: each step adds the cheapest candidate.
        std::vector<TopUpCandidate> added;
        int64_t worked = start;
        while (worked < min_minutes) {
            auto const candidates = top_up_candidates(data, sol, e, 0, may_assign);
            if (candidates.empty()) {
                break;
            }
            auto const& c = candidates.front();
            sol.assign(e, c.d, c.s);
            added.push_back(c);
            worked += data.shift_types[c.s].duration_minutes;
        }
        if (worked >= min_minutes) {
            continue;
        }

        // Greedy fell short, which does not mean the minimum is unreachable:
        // undo it and search.
        for (auto const& c : added) {
            sol.unassign(e, c.d);
        }
        int budget = kTopUpNodeBudget;
        if (!top_up_search(data, sol, e, 0, start, min_minutes, max_duration, may_assign, budget)) {
            // None found within the budget (or none exists): restore greedy's
            // partial top-up, which the search left undone.  The employee
            // stays short and the evaluator reports the roster infeasible.
            for (auto const& c : added) {
                sol.assign(e, c.d, c.s);
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  First-Fit-Decreasing
// ---------------------------------------------------------------------------

RosteringSolution construct_ffd(RosteringData const& data,
                                RosteringCostEvaluator const& evaluator) {
    RosteringSolution sol(data, evaluator);

    // Build list of (shift_type, day, demand) entries, one per demand.
    struct DemandEntry {
        int shift_type;
        int day;
        size_t index;  ///< Into data.get_demands(shift_type, day).
        int min_employees;
        RosteringData::Demand const* demand;
    };

    std::vector<DemandEntry> demands;
    int const ns = data.num_shift_types();
    int const H = data.horizon;

    for (int s = 0; s < ns; ++s) {
        for (int d = 0; d < H; ++d) {
            auto const cell = data.get_demands(s, d);
            for (size_t j = 0; j < cell.size(); ++j) {
                if (cell[j].min_employees > 0) {
                    demands.push_back({s, d, j, cell[j].min_employees, &cell[j]});
                }
            }
        }
    }

    // Sort by required employees descending (FFD ordering).
    std::sort(demands.begin(), demands.end(), [](DemandEntry const& a, DemandEntry const& b) {
        return a.min_employees > b.min_employees;
    });

    // For each demand entry, assign employees.
    for (auto const& dem : demands) {
        // Count how many of the cell's workers are credited to this demand.
        int assigned = credited_count(data, sol.schedule(), dem.shift_type, dem.day, dem.index);

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
                if (!qualifies(data, e, *dem.demand)) {
                    continue;
                }
                if (!is_feasible_assignment(data, sol.schedule(), e, dem.day, dem.shift_type)) {
                    continue;
                }
                sol.assign(e, dem.day, dem.shift_type);
                // Not always +1: the cell's crediting may move another
                // worker off this demand to cover one of its others.
                assigned = credited_count(data, sol.schedule(), dem.shift_type, dem.day, dem.index);
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
            auto const cell = data.get_demands(s, d);
            for (size_t j = 0; j < cell.size(); ++j) {
                auto const& dem = cell[j];
                if (dem.min_employees <= 0) {
                    continue;
                }

                // Count the cell's workers credited to this demand.
                int assigned = credited_count(data, sol.schedule(), s, d, j);

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
                        if (!qualifies(data, e, dem)) {
                            continue;
                        }
                        if (!is_feasible_assignment(data, sol.schedule(), e, d, s)) {
                            continue;
                        }
                        sol.assign(e, d, s);
                        assigned = credited_count(data, sol.schedule(), s, d, j);
                        found = true;
                        break;
                    }
                    if (!found) {
                        break;
                    }
                }
            }
        }
    }

    top_up_min_minutes(data, sol);
    return sol;
}

}  // namespace coso
