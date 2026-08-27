#include "assignment/replanning.h"

#include "assignment/construction.h"
#include "assignment/operators/block_swap.h"
#include "assignment/operators/shift_move.h"
#include "assignment/operators/shift_swap.h"

#include <algorithm>
#include <climits>
#include <numeric>

namespace coso {

// --------------------------------------------------------------------------- //
//  LockedCells                                                                 //
// --------------------------------------------------------------------------- //

LockedCells::LockedCells(ReplanConfig const& config, AssignmentSolution const& sol) {
    // Lock everything before horizon_start.
    for (int e = 0; e < sol.num_employees(); ++e) {
        for (int d = 0; d < config.horizon_start && d < sol.horizon(); ++d) {
            locked_.insert(key(e, d));
        }
    }

    // Lock explicitly specified assignments.
    for (auto const& [emp, day, shift] : config.locked_assignments) {
        locked_.insert(key(emp, day));
    }
}

// --------------------------------------------------------------------------- //
//  Helpers                                                                     //
// --------------------------------------------------------------------------- //

namespace {

/// Check whether assigning employee e to shift s on day d would violate
/// any hard constraint (unavailability, consecutive days, rest time).
/// Mirrors the logic from construction.cpp.
bool is_feasible_assignment(AssignmentData const& data,
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
    int max_consec = std::min(data.max_consecutive_shifts, data.employees[e].max_consecutive_days);
    int run_before = 0;
    for (int dd = d - 1; dd >= 0 && schedule[e][dd] >= 0; --dd) {
        ++run_before;
    }
    int run_after = 0;
    for (int dd = d + 1; dd < data.horizon && schedule[e][dd] >= 0; ++dd) {
        ++run_after;
    }
    if (run_before + 1 + run_after > max_consec) {
        return false;
    }

    // Minimum rest between shifts.
    int min_rest = std::max(data.min_rest_between_shifts, data.employees[e].min_rest_hours);
    if (min_rest > 0) {
        int ns = data.num_shift_types();
        if (d > 0) {
            int prev = schedule[e][d - 1];
            if (prev >= 0 && prev < ns) {
                int end_prev = data.shift_types[prev].end_hour;
                int start_cur = data.shift_types[s].start_hour;
                int rest = (24 - end_prev) + start_cur;
                if (rest < min_rest) {
                    return false;
                }
            }
        }
        if (d + 1 < data.horizon) {
            int next = schedule[e][d + 1];
            if (next >= 0 && next < ns) {
                int end_cur = data.shift_types[s].end_hour;
                int start_next = data.shift_types[next].start_hour;
                int rest = (24 - end_cur) + start_next;
                if (rest < min_rest) {
                    return false;
                }
            }
        }
    }

    // Forbidden sequences.
    for (auto const& seq : data.forbidden_sequences) {
        int len = static_cast<int>(seq.size());
        if (len < 2) {
            continue;
        }
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

    return true;
}

/// Check whether employee has a given skill (empty skill = always true).
bool employee_has_skill(AssignmentData const& data, int e, std::string const& skill) {
    if (skill.empty()) {
        return true;
    }
    auto const& skills = data.employees[e].skills;
    return std::find(skills.begin(), skills.end(), skill) != skills.end();
}

/// Greedy construction for unlocked cells only.
///
/// Iterates day by day, shift by shift, and assigns employees greedily to
/// unmet demand slots — but only for unlocked cells.
void construct_unlocked(AssignmentSolution& sol, AssignmentData const& data,
                        LockedCells const& locked) {
    int const ns = data.num_shift_types();
    int const H = data.horizon;
    int const ne = data.num_employees();

    for (int d = 0; d < H; ++d) {
        for (int s = 0; s < ns; ++s) {
            auto dem = data.get_demand(s, d);
            if (dem.min_employees <= 0) {
                continue;
            }

            // Count currently assigned employees (including locked ones).
            int assigned = 0;
            for (int e = 0; e < ne; ++e) {
                if (sol.get(e, d) == s && employee_has_skill(data, e, dem.required_skill)) {
                    ++assigned;
                }
            }

            // Sort candidates by load (fewest assigned days first).
            std::vector<int> candidates(ne);
            std::iota(candidates.begin(), candidates.end(), 0);
            std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
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
                    if (locked.is_locked(e, d)) {
                        continue;  // Cannot assign to locked cells.
                    }
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
}

/// Local search with locked-cell awareness.
///
/// Runs steepest-descent ShiftMove and ShiftSwap, but skips any move
/// that would modify a locked cell.
void local_search_with_locks(AssignmentSolution& sol, LockedCells const& locked) {
    bool improved = true;

    while (improved) {
        improved = false;

        // -- ShiftMove (with locking) --
        {
            ShiftMove::Move best;
            int const ne = sol.num_employees();
            int const H = sol.horizon();

            for (int d = 0; d < H; ++d) {
                for (int from = 0; from < ne; ++from) {
                    if (locked.is_locked(from, d)) {
                        continue;
                    }
                    int st = sol.get(from, d);
                    if (st < 0) {
                        continue;
                    }

                    for (int to = 0; to < ne; ++to) {
                        if (to == from) {
                            continue;
                        }
                        if (locked.is_locked(to, d)) {
                            continue;
                        }
                        if (sol.get(to, d) >= 0) {
                            continue;
                        }

                        int delta = ShiftMove::evaluate(sol, from, to, d);
                        if (delta < best.delta) {
                            best = ShiftMove::Move{from, to, d, st, delta};
                        }
                    }
                }
            }

            if (best.delta < 0) {
                sol.unassign(best.from_emp, best.day);
                sol.assign(best.to_emp, best.day, best.shift_type);
                improved = true;
                continue;
            }
        }

        // -- ShiftSwap (with locking) --
        {
            ShiftSwap::Move best;
            int const ne = sol.num_employees();
            int const H = sol.horizon();

            for (int d = 0; d < H; ++d) {
                for (int e1 = 0; e1 < ne; ++e1) {
                    if (locked.is_locked(e1, d)) {
                        continue;
                    }
                    for (int e2 = e1 + 1; e2 < ne; ++e2) {
                        if (locked.is_locked(e2, d)) {
                            continue;
                        }
                        if (sol.get(e1, d) == sol.get(e2, d)) {
                            continue;
                        }

                        int delta = ShiftSwap::evaluate(sol, e1, e2, d);
                        if (delta < best.delta) {
                            best = ShiftSwap::Move{e1, e2, d, delta};
                        }
                    }
                }
            }

            if (best.delta < 0) {
                sol.swap(best.emp1, best.emp2, best.day);
                improved = true;
                continue;
            }
        }

        // -- BlockSwap (with locking) --
        {
            BlockSwap::Move best;
            int const ne = sol.num_employees();
            int const H = sol.horizon();
            int const max_block_len = 7;

            for (int e1 = 0; e1 < ne; ++e1) {
                for (int e2 = e1 + 1; e2 < ne; ++e2) {
                    for (int start = 0; start < H; ++start) {
                        int max_len = std::min(max_block_len, H - start);
                        for (int len = 2; len <= max_len; ++len) {
                            // Check no day in the block is locked for either
                            // employee.
                            bool any_locked = false;
                            bool differs = false;
                            for (int d = start; d < start + len; ++d) {
                                if (locked.is_locked(e1, d) || locked.is_locked(e2, d)) {
                                    any_locked = true;
                                    break;
                                }
                                if (sol.get(e1, d) != sol.get(e2, d)) {
                                    differs = true;
                                }
                            }
                            if (any_locked || !differs) {
                                continue;
                            }

                            int delta = BlockSwap::evaluate(sol, e1, e2, start, len);
                            if (delta < best.delta) {
                                best = BlockSwap::Move{e1, e2, start, len, delta};
                            }
                        }
                    }
                }
            }

            if (best.delta < 0) {
                for (int d = best.start; d < best.start + best.len; ++d) {
                    sol.swap(best.emp1, best.emp2, d);
                }
                improved = true;
                continue;
            }
        }
    }
}

}  // anonymous namespace

// --------------------------------------------------------------------------- //
//  replan                                                                      //
// --------------------------------------------------------------------------- //

void replan(AssignmentSolution& sol, ReplanConfig const& config, AssignmentData& data,
            AssignmentCostEvaluator const& evaluator) {
    // 1. Apply new constraints to the data.
    for (auto const& [emp, day] : config.new_unavailabilities) {
        data.unavailabilities.insert(AssignmentData::unavail_key(emp, day));
    }
    for (auto const& pref : config.new_preferences) {
        data.preferences.push_back(pref);
    }

    // 2. Build locked cells.
    LockedCells locked(config, sol);

    // 3. Set locked assignments to the specified shift types.
    //    (They may already be correct, but ensure they match.)
    for (auto const& [emp, day, shift] : config.locked_assignments) {
        if (sol.get(emp, day) != shift) {
            if (sol.get(emp, day) >= 0) {
                sol.unassign(emp, day);
            }
            if (shift >= 0) {
                sol.assign(emp, day, shift);
            }
        }
    }

    // 4. Clear unlocked cells that violate new constraints
    //    (e.g., newly unavailable employees).
    for (int e = 0; e < sol.num_employees(); ++e) {
        for (int d = 0; d < sol.horizon(); ++d) {
            if (locked.is_locked(e, d)) {
                continue;
            }
            if (sol.get(e, d) >= 0 && data.is_unavailable(e, d)) {
                sol.unassign(e, d);
            }
        }
    }

    // 5. Re-run greedy construction for unassigned, unlocked slots.
    construct_unlocked(sol, data, locked);

    // 6. Run local search, skipping moves that affect locked cells.
    local_search_with_locks(sol, locked);

    // 7. Recompute cost from scratch to ensure consistency.
    sol.recompute_cost();
}

}  // namespace coso
