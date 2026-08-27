#pragma once

#include "assignment/assignment_data.h"
#include "assignment/assignment_solution.h"

#include <optional>

namespace coso {

/// Swap the shift assignments of two employees on the same day.
///
/// Enumerates all (emp1, emp2, day) triples where the two employees have
/// different assignments.  Uses delta evaluation to score each move without
/// modifying the solution.
///
/// Designed for steepest-descent local search: find_best_move() returns true
/// if an improving move exists; apply() executes it.
class ShiftSwap {
public:
    struct Move {
        int emp1 = -1;
        int emp2 = -1;
        int day = -1;
        int delta = 0;
    };

    /// Scan all swap moves and find the best improving one.
    ///
    /// @return true if an improving move was found (delta < 0).
    [[nodiscard]] bool find_best_move(AssignmentSolution const& sol) {
        best_ = Move{};
        int const ne = sol.num_employees();
        int const H = sol.horizon();

        for (int d = 0; d < H; ++d) {
            for (int e1 = 0; e1 < ne; ++e1) {
                for (int e2 = e1 + 1; e2 < ne; ++e2) {
                    // Skip if both have the same assignment (swap is no-op).
                    if (sol.get(e1, d) == sol.get(e2, d)) {
                        continue;
                    }

                    int delta = evaluate(sol, e1, e2, d);
                    if (delta < best_.delta) {
                        best_ = Move{e1, e2, d, delta};
                    }
                }
            }
        }

        return best_.delta < 0;
    }

    /// Apply the stored best move to the solution.
    /// Precondition: find_best_move() returned true.
    void apply(AssignmentSolution& sol) const { sol.swap(best_.emp1, best_.emp2, best_.day); }

    /// Evaluate the cost delta of swapping emp1 and emp2 on day.
    ///
    /// This performs the swap, records the delta, then undoes it so that
    /// the solution is unchanged.
    [[nodiscard]] static int evaluate(AssignmentSolution const& sol, int emp1, int emp2, int day) {
        // Use a const_cast to perform a temporary swap and revert.
        // The solution state is identical before and after.
        auto& mut = const_cast<AssignmentSolution&>(sol);
        int delta = mut.swap(emp1, emp2, day);
        // Undo: swap back.
        mut.swap(emp1, emp2, day);
        return delta;
    }

    /// The best move found (valid only after find_best_move() returns true).
    [[nodiscard]] Move const& best_move() const noexcept { return best_; }

    /// The cost delta of the best move found.
    [[nodiscard]] int best_delta() const noexcept { return best_.delta; }

    /// Enumerate all valid swap moves (for testing / exhaustive search).
    [[nodiscard]] static std::vector<Move> enumerate(AssignmentSolution const& sol) {
        std::vector<Move> moves;
        int const ne = sol.num_employees();
        int const H = sol.horizon();

        for (int d = 0; d < H; ++d) {
            for (int e1 = 0; e1 < ne; ++e1) {
                for (int e2 = e1 + 1; e2 < ne; ++e2) {
                    if (sol.get(e1, d) == sol.get(e2, d)) {
                        continue;
                    }
                    int delta = evaluate(sol, e1, e2, d);
                    moves.push_back(Move{e1, e2, d, delta});
                }
            }
        }
        return moves;
    }

private:
    Move best_;
};

}  // namespace coso
