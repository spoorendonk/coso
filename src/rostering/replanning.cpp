#include "rostering/replanning.h"

#include "rostering/construction.h"
#include "rostering/cover.h"
#include "rostering/operators/block_swap.h"
#include "rostering/operators/shift_move.h"
#include "rostering/operators/shift_swap.h"

#include <algorithm>
#include <climits>
#include <numeric>

namespace coso {

// --------------------------------------------------------------------------- //
//  LockedCells                                                                 //
// --------------------------------------------------------------------------- //

LockedCells::LockedCells(ReplanConfig const& config, RosteringSolution const& sol) {
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

/// Greedy construction for unlocked cells only.
///
/// Iterates day by day, shift by shift, and assigns employees greedily to
/// unmet demand slots — but only for unlocked cells.  Assignments go through
/// the constructions' is_feasible_assignment, so the upper bounds hold; there
/// is no min_total_minutes top-up here, so a short employee is left to the
/// evaluator to report.
void construct_unlocked(RosteringSolution& sol, RosteringData const& data,
                        LockedCells const& locked) {
    int const ns = data.num_shift_types();
    int const H = data.horizon;
    int const ne = data.num_employees();

    for (int d = 0; d < H; ++d) {
        for (int s = 0; s < ns; ++s) {
            auto const cell = data.get_demands(s, d);
            for (size_t j = 0; j < cell.size(); ++j) {
                auto const& dem = cell[j];
                if (dem.min_employees <= 0) {
                    continue;
                }

                // Count the cell's workers credited to this demand (locked ones
                // included).
                int assigned = credited_count(data, sol.schedule(), s, d, j);

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
}

/// Local search with locked-cell awareness.
///
/// Runs steepest-descent ShiftMove and ShiftSwap, but skips any move
/// that would modify a locked cell.
void local_search_with_locks(RosteringSolution& sol, LockedCells const& locked) {
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

void replan(RosteringSolution& sol, ReplanConfig const& config, RosteringData& data,
            RosteringCostEvaluator const& evaluator) {
    // 1. Apply new constraints to the data.
    for (auto const& [emp, day] : config.new_unavailabilities) {
        data.unavailabilities.insert(RosteringData::unavail_key(emp, day));
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
