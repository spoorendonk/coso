#pragma once

#include "assignment/assignment_data.h"
#include "assignment/assignment_solution.h"

#include <algorithm>
#include <vector>

namespace coso {

/// Swap a block of consecutive days between two employees.
///
/// For each pair of employees and each contiguous block of days [start, start+len),
/// evaluates swapping all assignments in that block.  The block must differ in
/// at least one day (otherwise it is a no-op).
///
/// Designed for steepest-descent local search: find_best_move() returns true
/// if an improving move exists; apply() executes it.
class BlockSwap {
public:
    struct Move {
        int emp1 = -1;
        int emp2 = -1;
        int start = -1;  ///< First day of the block.
        int len = -1;    ///< Number of consecutive days.
        int delta = 0;
    };

    /// Maximum block length to consider (keeps neighbourhood tractable).
    int max_block_len = 7;

    /// Scan all block-swap moves and find the best improving one.
    ///
    /// @return true if an improving move was found (delta < 0).
    [[nodiscard]] bool find_best_move(AssignmentSolution const& sol) {
        best_ = Move{};
        int const ne = sol.num_employees();
        int const H = sol.horizon();

        for (int e1 = 0; e1 < ne; ++e1) {
            for (int e2 = e1 + 1; e2 < ne; ++e2) {
                for (int start = 0; start < H; ++start) {
                    int max_len = std::min(max_block_len, H - start);
                    for (int len = 2; len <= max_len; ++len) {
                        // Check that the block differs in at least one day.
                        bool differs = false;
                        for (int d = start; d < start + len; ++d) {
                            if (sol.get(e1, d) != sol.get(e2, d)) {
                                differs = true;
                                break;
                            }
                        }
                        if (!differs) {
                            continue;
                        }

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

    /// Apply the stored best move to the solution.
    /// Precondition: find_best_move() returned true.
    void apply(AssignmentSolution& sol) const {
        for (int d = best_.start; d < best_.start + best_.len; ++d) {
            sol.swap(best_.emp1, best_.emp2, d);
        }
    }

    /// Evaluate the cost delta of swapping a block [start, start+len)
    /// between emp1 and emp2.
    ///
    /// Temporarily applies all swaps in the block and reverts them.
    [[nodiscard]] static int evaluate(AssignmentSolution const& sol, int emp1, int emp2, int start,
                                      int len) {
        auto& mut = const_cast<AssignmentSolution&>(sol);

        // Apply all swaps in the block, accumulating the delta.
        int delta = 0;
        for (int d = start; d < start + len; ++d) {
            delta += mut.swap(emp1, emp2, d);
        }

        // Undo all swaps (reverse order for symmetry, though swap is its own
        // inverse regardless of order).
        for (int d = start + len - 1; d >= start; --d) {
            mut.swap(emp1, emp2, d);
        }

        return delta;
    }

    /// The best move found.
    [[nodiscard]] Move const& best_move() const noexcept { return best_; }

    /// The cost delta of the best move found.
    [[nodiscard]] int best_delta() const noexcept { return best_.delta; }

    /// Enumerate all valid block-swap moves.
    [[nodiscard]] static std::vector<Move> enumerate(AssignmentSolution const& sol,
                                                     int max_len = 7) {
        std::vector<Move> moves;
        int const ne = sol.num_employees();
        int const H = sol.horizon();

        for (int e1 = 0; e1 < ne; ++e1) {
            for (int e2 = e1 + 1; e2 < ne; ++e2) {
                for (int start = 0; start < H; ++start) {
                    int ml = std::min(max_len, H - start);
                    for (int len = 2; len <= ml; ++len) {
                        bool differs = false;
                        for (int d = start; d < start + len; ++d) {
                            if (sol.get(e1, d) != sol.get(e2, d)) {
                                differs = true;
                                break;
                            }
                        }
                        if (!differs) {
                            continue;
                        }

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

}  // namespace coso
