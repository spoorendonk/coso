#pragma once

#include "assignment/assignment_data.h"
#include "assignment/assignment_solution.h"

#include <algorithm>
#include <vector>

namespace coso {

/// Move a column (block of consecutive days) of shifts from one employee to
/// another.  For each day in the block, the source employee's shift is
/// transferred to the target (source becomes unassigned).  Only considers
/// targets that are unassigned on *all* days of the block.
///
/// Designed for steepest-descent local search.
class PillarMove {
public:
    struct Move {
        int from_emp = -1;
        int to_emp   = -1;
        int start    = -1;   ///< First day of the block.
        int len      = -1;   ///< Number of consecutive days.
        int delta    = 0;
    };

    /// Maximum block length to consider.
    int max_block_len = 7;

    /// Scan all pillar-move operations and find the best improving one.
    ///
    /// @return true if an improving move was found (delta < 0).
    [[nodiscard]] bool find_best_move(AssignmentSolution const& sol)
    {
        best_ = Move{};
        int const ne = sol.num_employees();
        int const H  = sol.horizon();

        for (int from = 0; from < ne; ++from) {
            for (int start = 0; start < H; ++start) {
                // Source must have a shift on the first day.
                if (sol.get(from, start) < 0)
                    continue;

                // Determine maximum contiguous assigned block from `start`.
                int max_len = std::min(max_block_len, H - start);
                int block_len = 0;
                for (int d = start; d < start + max_len; ++d) {
                    if (sol.get(from, d) < 0)
                        break;
                    ++block_len;
                }
                if (block_len < 1)
                    continue;

                for (int to = 0; to < ne; ++to) {
                    if (to == from)
                        continue;

                    // Check that target is unassigned on all days of the block.
                    for (int len = 1; len <= block_len; ++len) {
                        // Target must be free on the newly added day.
                        if (sol.get(to, start + len - 1) >= 0)
                            break;

                        int delta = evaluate(sol, from, to, start, len);
                        if (delta < best_.delta) {
                            best_ = Move{from, to, start, len, delta};
                        }
                    }
                }
            }
        }

        return best_.delta < 0;
    }

    /// Apply the stored best move to the solution.
    void apply(AssignmentSolution& sol) const
    {
        for (int d = best_.start; d < best_.start + best_.len; ++d) {
            int st = sol.get(best_.from_emp, d);
            sol.unassign(best_.from_emp, d);
            sol.assign(best_.to_emp, d, st);
        }
    }

    /// Evaluate the cost delta of moving a block [start, start+len) from
    /// from_emp to to_emp.
    [[nodiscard]] static int evaluate(AssignmentSolution const& sol,
                                      int from_emp, int to_emp,
                                      int start, int len)
    {
        auto& mut = const_cast<AssignmentSolution&>(sol);

        // Record shift types and apply.
        std::vector<int> shift_types(len);
        int delta = 0;
        for (int i = 0; i < len; ++i) {
            int d = start + i;
            shift_types[i] = sol.get(from_emp, d);
            delta += mut.unassign(from_emp, d);
            delta += mut.assign(to_emp, d, shift_types[i]);
        }

        // Undo (reverse order).
        for (int i = len - 1; i >= 0; --i) {
            int d = start + i;
            mut.unassign(to_emp, d);
            mut.assign(from_emp, d, shift_types[i]);
        }

        return delta;
    }

    [[nodiscard]] Move const& best_move() const noexcept { return best_; }
    [[nodiscard]] int best_delta() const noexcept { return best_.delta; }

    /// Enumerate all valid pillar-move operations.
    [[nodiscard]] static std::vector<Move> enumerate(
        AssignmentSolution const& sol, int max_len = 7)
    {
        std::vector<Move> moves;
        int const ne = sol.num_employees();
        int const H  = sol.horizon();

        for (int from = 0; from < ne; ++from) {
            for (int start = 0; start < H; ++start) {
                if (sol.get(from, start) < 0)
                    continue;

                int ml = std::min(max_len, H - start);
                int block_len = 0;
                for (int d = start; d < start + ml; ++d) {
                    if (sol.get(from, d) < 0)
                        break;
                    ++block_len;
                }

                for (int to = 0; to < ne; ++to) {
                    if (to == from)
                        continue;

                    for (int len = 1; len <= block_len; ++len) {
                        if (sol.get(to, start + len - 1) >= 0)
                            break;

                        int delta = evaluate(sol, from, to, start, len);
                        moves.push_back(Move{from, to, start, len, delta});
                    }
                }
            }
        }
        return moves;
    }

private:
    Move best_;
};

/// Swap columns (blocks of consecutive days) between two employees.
///
/// Unlike BlockSwap (which swaps individual days), PillarSwap operates on
/// contiguous "pillars" where both employees have shifts assigned.  The
/// block must have at least one day where assignments differ.
///
/// Designed for steepest-descent local search.
class PillarSwap {
public:
    struct Move {
        int emp1  = -1;
        int emp2  = -1;
        int start = -1;
        int len   = -1;
        int delta = 0;
    };

    int max_block_len = 7;

    /// Scan all pillar-swap moves and find the best improving one.
    [[nodiscard]] bool find_best_move(AssignmentSolution const& sol)
    {
        best_ = Move{};
        int const ne = sol.num_employees();
        int const H  = sol.horizon();

        for (int e1 = 0; e1 < ne; ++e1) {
            for (int e2 = e1 + 1; e2 < ne; ++e2) {
                for (int start = 0; start < H; ++start) {
                    // Both must be assigned on the first day.
                    if (sol.get(e1, start) < 0 || sol.get(e2, start) < 0)
                        continue;

                    int max_len = std::min(max_block_len, H - start);
                    for (int len = 1; len <= max_len; ++len) {
                        int d = start + len - 1;
                        // Both must be assigned on each day of the block.
                        if (sol.get(e1, d) < 0 || sol.get(e2, d) < 0)
                            break;

                        // At least one day must differ.
                        bool differs = false;
                        for (int dd = start; dd < start + len; ++dd) {
                            if (sol.get(e1, dd) != sol.get(e2, dd)) {
                                differs = true;
                                break;
                            }
                        }
                        if (!differs)
                            continue;

                        int delta = evaluate(sol, e1, e2, start, len);
                        if (delta < best_.delta) {
                            best_ = Move{e1, e2, start, len, delta};
                        }
                    }
                }
            }
        }

        return best_.delta < 0;
    }

    /// Apply the stored best move.
    void apply(AssignmentSolution& sol) const
    {
        for (int d = best_.start; d < best_.start + best_.len; ++d) {
            sol.swap(best_.emp1, best_.emp2, d);
        }
    }

    /// Evaluate cost delta of swapping the block [start, start+len).
    [[nodiscard]] static int evaluate(AssignmentSolution const& sol,
                                      int emp1, int emp2,
                                      int start, int len)
    {
        auto& mut = const_cast<AssignmentSolution&>(sol);

        int delta = 0;
        for (int d = start; d < start + len; ++d) {
            delta += mut.swap(emp1, emp2, d);
        }

        // Undo.
        for (int d = start + len - 1; d >= start; --d) {
            mut.swap(emp1, emp2, d);
        }

        return delta;
    }

    [[nodiscard]] Move const& best_move() const noexcept { return best_; }
    [[nodiscard]] int best_delta() const noexcept { return best_.delta; }

    /// Enumerate all valid pillar-swap moves.
    [[nodiscard]] static std::vector<Move> enumerate(
        AssignmentSolution const& sol, int max_len = 7)
    {
        std::vector<Move> moves;
        int const ne = sol.num_employees();
        int const H  = sol.horizon();

        for (int e1 = 0; e1 < ne; ++e1) {
            for (int e2 = e1 + 1; e2 < ne; ++e2) {
                for (int start = 0; start < H; ++start) {
                    if (sol.get(e1, start) < 0 || sol.get(e2, start) < 0)
                        continue;

                    int ml = std::min(max_len, H - start);
                    for (int len = 1; len <= ml; ++len) {
                        int d = start + len - 1;
                        if (sol.get(e1, d) < 0 || sol.get(e2, d) < 0)
                            break;

                        bool differs = false;
                        for (int dd = start; dd < start + len; ++dd) {
                            if (sol.get(e1, dd) != sol.get(e2, dd)) {
                                differs = true;
                                break;
                            }
                        }
                        if (!differs)
                            continue;

                        int delta = evaluate(sol, e1, e2, start, len);
                        moves.push_back(Move{e1, e2, start, len, delta});
                    }
                }
            }
        }
        return moves;
    }

private:
    Move best_;
};

/// Cyclic rotation of shifts among 3+ employees for a day or block of days.
///
/// For a given set of employees [e0, e1, ..., eK] and a day/block:
///   e0 gets e1's shifts, e1 gets e2's shifts, ..., eK gets e0's shifts.
///
/// Enumerates all triples (e0, e1, e2) for single days and blocks.
/// Larger rotations could be enumerated but triples are the practical sweet
/// spot for neighbourhood size.
///
/// Designed for steepest-descent local search.
class PillarRotate {
public:
    struct Move {
        int emp0  = -1;   ///< First employee in the rotation cycle.
        int emp1  = -1;   ///< Second employee.
        int emp2  = -1;   ///< Third employee.
        int start = -1;
        int len   = -1;
        int delta = 0;
    };

    int max_block_len = 3;

    /// Scan all 3-employee rotation moves and find the best improving one.
    [[nodiscard]] bool find_best_move(AssignmentSolution const& sol)
    {
        best_ = Move{};
        int const ne = sol.num_employees();
        int const H  = sol.horizon();

        for (int e0 = 0; e0 < ne; ++e0) {
            for (int e1 = e0 + 1; e1 < ne; ++e1) {
                for (int e2 = e1 + 1; e2 < ne; ++e2) {
                    for (int start = 0; start < H; ++start) {
                        int max_len = std::min(max_block_len, H - start);
                        for (int len = 1; len <= max_len; ++len) {
                            // Check that the rotation is non-trivial: at least
                            // one day where not all three have the same shift.
                            bool nontrivial = false;
                            for (int d = start; d < start + len; ++d) {
                                int s0 = sol.get(e0, d);
                                int s1 = sol.get(e1, d);
                                int s2 = sol.get(e2, d);
                                if (s0 != s1 || s1 != s2) {
                                    nontrivial = true;
                                    break;
                                }
                            }
                            if (!nontrivial)
                                continue;

                            // Try both rotation directions.
                            int d_fwd = evaluate(sol, e0, e1, e2, start, len);
                            if (d_fwd < best_.delta) {
                                best_ = Move{e0, e1, e2, start, len, d_fwd};
                            }

                            int d_bwd = evaluate(sol, e0, e2, e1, start, len);
                            if (d_bwd < best_.delta) {
                                best_ = Move{e0, e2, e1, start, len, d_bwd};
                            }
                        }
                    }
                }
            }
        }

        return best_.delta < 0;
    }

    /// Apply the stored best move (cyclic rotation: e0<-e1, e1<-e2, e2<-e0).
    void apply(AssignmentSolution& sol) const
    {
        apply_rotation(sol, best_.emp0, best_.emp1, best_.emp2,
                       best_.start, best_.len);
    }

    /// Apply a cyclic rotation: e0 gets e1's shift, e1 gets e2's, e2 gets e0's.
    static void apply_rotation(AssignmentSolution& sol,
                               int e0, int e1, int e2,
                               int start, int len)
    {
        for (int d = start; d < start + len; ++d) {
            int s0 = sol.get(e0, d);
            int s1 = sol.get(e1, d);
            int s2 = sol.get(e2, d);

            // Unassign all three.
            if (s0 >= 0) sol.unassign(e0, d);
            if (s1 >= 0) sol.unassign(e1, d);
            if (s2 >= 0) sol.unassign(e2, d);

            // Reassign in rotated order: e0 <- s1, e1 <- s2, e2 <- s0.
            if (s1 >= 0) sol.assign(e0, d, s1);
            if (s2 >= 0) sol.assign(e1, d, s2);
            if (s0 >= 0) sol.assign(e2, d, s0);
        }
    }

    /// Evaluate the cost delta of rotating (e0<-e1, e1<-e2, e2<-e0)
    /// on the block [start, start+len).
    [[nodiscard]] static int evaluate(AssignmentSolution const& sol,
                                      int e0, int e1, int e2,
                                      int start, int len)
    {
        auto& mut = const_cast<AssignmentSolution&>(sol);
        int cost_before = sol.cost();

        apply_rotation(mut, e0, e1, e2, start, len);
        int cost_after = sol.cost();

        // Undo: reverse rotation is (e0<-e2, e2<-e1, e1<-e0) = rotate(e0,e2,e1).
        apply_rotation(mut, e0, e2, e1, start, len);

        return cost_after - cost_before;
    }

    [[nodiscard]] Move const& best_move() const noexcept { return best_; }
    [[nodiscard]] int best_delta() const noexcept { return best_.delta; }

    /// Enumerate all valid 3-employee rotation moves.
    [[nodiscard]] static std::vector<Move> enumerate(
        AssignmentSolution const& sol, int max_len = 3)
    {
        std::vector<Move> moves;
        int const ne = sol.num_employees();
        int const H  = sol.horizon();

        for (int e0 = 0; e0 < ne; ++e0) {
            for (int e1 = e0 + 1; e1 < ne; ++e1) {
                for (int e2 = e1 + 1; e2 < ne; ++e2) {
                    for (int start = 0; start < H; ++start) {
                        int ml = std::min(max_len, H - start);
                        for (int len = 1; len <= ml; ++len) {
                            bool nontrivial = false;
                            for (int d = start; d < start + len; ++d) {
                                int s0 = sol.get(e0, d);
                                int s1 = sol.get(e1, d);
                                int s2 = sol.get(e2, d);
                                if (s0 != s1 || s1 != s2) {
                                    nontrivial = true;
                                    break;
                                }
                            }
                            if (!nontrivial)
                                continue;

                            // Forward rotation.
                            int d_fwd = evaluate(sol, e0, e1, e2, start, len);
                            moves.push_back(
                                Move{e0, e1, e2, start, len, d_fwd});

                            // Backward rotation.
                            int d_bwd = evaluate(sol, e0, e2, e1, start, len);
                            moves.push_back(
                                Move{e0, e2, e1, start, len, d_bwd});
                        }
                    }
                }
            }
        }
        return moves;
    }

private:
    Move best_;
};

/// VND-style pillar search: try PillarMove, PillarSwap, PillarRotate in
/// sequence.  On any improvement, restart from PillarMove.
///
/// @return total cost improvement (sum of deltas applied).
inline int pillar_vnd(AssignmentSolution& sol,
                      int max_block_len = 7,
                      int max_rotate_block = 3)
{
    int total_delta = 0;
    bool improved = true;

    PillarMove  pm;
    PillarSwap  ps;
    PillarRotate pr;
    pm.max_block_len = max_block_len;
    ps.max_block_len = max_block_len;
    pr.max_block_len = max_rotate_block;

    while (improved) {
        improved = false;

        // Level 1: PillarMove.
        while (pm.find_best_move(sol)) {
            pm.apply(sol);
            total_delta += pm.best_delta();
            improved = true;
        }

        // Level 2: PillarSwap.
        if (ps.find_best_move(sol)) {
            ps.apply(sol);
            total_delta += ps.best_delta();
            improved = true;
            continue;  // Restart from level 1.
        }

        // Level 3: PillarRotate.
        if (pr.find_best_move(sol)) {
            pr.apply(sol);
            total_delta += pr.best_delta();
            improved = true;
            continue;  // Restart from level 1.
        }
    }

    return total_delta;
}

} // namespace coso
