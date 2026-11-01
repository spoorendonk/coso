#include "rostering/construction.h"

#include "rostering/cover.h"

#include <algorithm>
#include <climits>
#include <cstddef>
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

/// One employee's top-up, run on a plain copy of the schedule so the search
/// pays no cost re-evaluation per node; what it settles on is applied to the
/// solution once.
struct TopUp {
    RosteringData const& data;
    std::vector<std::vector<int>>& sched;
    int e;
    std::vector<char> open;           ///< Day d was free, available and allowed.
    std::vector<int64_t> open_after;  ///< open days in [d, horizon).
    int64_t min_minutes;
    int64_t cap;  ///< max_total_minutes: no top-up goes past it.
    int max_duration;
    int budget;

    /// Every (day, shift) from `from_day` on that the top-up may give e:
    /// open, feasible, adding minutes, never a hard over-cover.  Each carries
    /// the change in its cell's cover cost, with workers credited as
    /// rostering/cover.h describes, so a shortfall fill scores lowest, then an
    /// assignment within max_employees, then a soft over-cover.
    ///
    /// With `one_per_duration`, a day keeps only its cheapest shift of each
    /// length.  Without forbidden sequences, shifts of equal length on one day
    /// leave the same later choices open (a hard over-cover depends only on
    /// other employees' cells), so the rest only multiply the search tree.
    std::vector<TopUpCandidate> candidates(int from_day, bool one_per_duration) const {
        std::vector<TopUpCandidate> out;
        for (int d = from_day; d < data.horizon; ++d) {
            if (open[d] == 0) {
                continue;
            }
            std::size_t const day_begin = out.size();
            for (int s = 0; s < data.num_shift_types(); ++s) {
                // A shift adding no minutes cannot close the gap.
                if (data.shift_types[s].duration_minutes <= 0 ||
                    !is_feasible_assignment(data, sched, e, d, s)) {
                    continue;
                }
                auto workers = cell_workers(sched, s, d);
                auto const [soft_before, hard_before] = cell_cover_cost(data, s, d, workers);
                workers.push_back(e);
                auto const [soft_after, hard_after] = cell_cover_cost(data, s, d, workers);
                if (hard_after > hard_before) {
                    continue;  // Never a hard over-cover.
                }
                TopUpCandidate const c{d, s,
                                       static_cast<int64_t>(soft_after - soft_before) +
                                           static_cast<int64_t>(hard_after - hard_before)};
                if (one_per_duration) {
                    auto const same =
                        std::find_if(out.begin() + static_cast<std::ptrdiff_t>(day_begin),
                                     out.end(), [&](TopUpCandidate const& o) {
                                         return data.shift_types[o.s].duration_minutes ==
                                                data.shift_types[s].duration_minutes;
                                     });
                    if (same != out.end()) {
                        if (c.score < same->score) {
                            *same = c;
                        }
                        continue;
                    }
                }
                out.push_back(c);
            }
        }
        return out;
    }

    /// Depth-first search for extra shifts taking e from `worked` to the
    /// minimum, added in day order from `from_day`.  Every hard bound
    /// is_feasible_assignment checks only tightens as shifts are added, so a
    /// set passes it in day order if it passes in any order, and day order
    /// loses no solution.  The search only fills days from `from_day` on, so
    /// open_after stays exact.  On success `found` holds the shifts and they
    /// stay in `sched`; on failure `sched` is as it was.
    bool search(int from_day, int64_t worked, std::vector<TopUpCandidate>& found) {
        if (worked >= min_minutes) {
            return true;
        }
        if (--budget < 0) {
            return false;
        }
        // Prune: a longest shift on every remaining open day, capped by
        // max_total_minutes, still falls short.
        int64_t const reach = std::min(worked + open_after[from_day] * max_duration, cap);
        if (reach < min_minutes) {
            return false;
        }
        auto cands = candidates(from_day, data.forbidden_sequences.empty());
        std::stable_sort(
            cands.begin(), cands.end(),
            [](TopUpCandidate const& a, TopUpCandidate const& b) { return a.score < b.score; });
        for (auto const& c : cands) {
            sched[e][c.d] = c.s;
            found.push_back(c);
            if (search(c.d + 1, worked + data.shift_types[c.s].duration_minutes, found)) {
                return true;
            }
            found.pop_back();
            sched[e][c.d] = -1;
            if (budget < 0) {
                return false;
            }
        }
        return false;
    }
};

/// Nodes the backtracking top-up may expand per employee before it gives up.
constexpr int kTopUpNodeBudget = 20000;

}  // anonymous namespace

void top_up_min_minutes(RosteringData const& data, RosteringSolution& sol,
                        CellFilter const& may_assign) {
    int max_duration = 0;
    for (auto const& st : data.shift_types) {
        max_duration = std::max(max_duration, st.duration_minutes);
    }
    int const H = data.horizon;

    for (int e = 0; e < data.num_employees(); ++e) {
        int64_t const min_minutes = data.employees[e].min_total_minutes;
        int64_t const start = worked_minutes(data, sol.schedule(), e);
        if (start >= min_minutes) {
            continue;
        }

        auto sched = sol.schedule();
        TopUp t{.data = data,
                .sched = sched,
                .e = e,
                .open = std::vector<char>(H, 0),
                .open_after = std::vector<int64_t>(H + 1, 0),
                .min_minutes = min_minutes,
                .cap = data.employees[e].max_total_minutes,
                .max_duration = max_duration,
                .budget = kTopUpNodeBudget};
        for (int d = H - 1; d >= 0; --d) {
            t.open[d] = static_cast<char>(sched[e][d] < 0 && !data.is_unavailable(e, d) &&
                                          (!may_assign || may_assign(e, d)));
            t.open_after[d] = t.open_after[d + 1] + t.open[d];
        }

        // Greedy first: each step adds the cheapest candidate, earliest day
        // and lowest shift on ties.
        std::vector<TopUpCandidate> greedy;
        int64_t worked = start;
        while (worked < min_minutes) {
            auto const cands = t.candidates(0, false);
            auto const best = std::min_element(
                cands.begin(), cands.end(),
                [](TopUpCandidate const& a, TopUpCandidate const& b) { return a.score < b.score; });
            if (best == cands.end()) {
                break;
            }
            sched[e][best->d] = best->s;
            greedy.push_back(*best);
            worked += data.shift_types[best->s].duration_minutes;
        }

        // Greedy falling short does not mean the minimum is unreachable:
        // undo it and search, unless max_total_minutes rules the minimum out.
        std::vector<TopUpCandidate> found;
        if (worked < min_minutes && min_minutes <= t.cap) {
            for (auto const& c : greedy) {
                sched[e][c.d] = -1;
            }
            if (!t.search(0, start, found)) {
                found.clear();
            }
        }
        // Without a search result keep greedy's (possibly partial) top-up:
        // the employee stays short and the evaluator reports the roster
        // infeasible.
        for (auto const& c : found.empty() ? greedy : found) {
            sol.assign(e, c.d, c.s);
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
