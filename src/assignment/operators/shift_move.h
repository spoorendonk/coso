#pragma once

#include "assignment/assignment_data.h"
#include "assignment/assignment_solution.h"

#include <vector>

namespace coso {

/// Move a shift from one employee to another on the same day.
///
/// For each (employee, day) with an assigned shift, evaluates moving that
/// shift to every other employee on the same day (unassigning the source,
/// assigning the target).  Only considers targets that are currently
/// unassigned on that day (to avoid overwriting existing assignments).
///
/// Designed for steepest-descent local search: find_best_move() returns true
/// if an improving move exists; apply() executes it.
class ShiftMove {
public:
    struct Move {
        int from_emp = -1;
        int to_emp = -1;
        int day = -1;
        int shift_type = -1;
        int delta = 0;
    };

    /// Scan all move operations and find the best improving one.
    ///
    /// @return true if an improving move was found (delta < 0).
    [[nodiscard]] bool find_best_move(AssignmentSolution const& sol) {
        best_ = Move{};
        int const ne = sol.num_employees();
        int const H = sol.horizon();

        for (int d = 0; d < H; ++d) {
            for (int from = 0; from < ne; ++from) {
                int st = sol.get(from, d);
                if (st < 0) {
                    continue;  // Nothing to move.
                }

                for (int to = 0; to < ne; ++to) {
                    if (to == from) {
                        continue;
                    }
                    if (sol.get(to, d) >= 0) {
                        continue;  // Target already has a shift.
                    }

                    int delta = evaluate(sol, from, to, d);
                    if (delta < best_.delta) {
                        best_ = Move{from, to, d, st, delta};
                    }
                }
            }
        }

        return best_.delta < 0;
    }

    /// Apply the stored best move to the solution.
    /// Precondition: find_best_move() returned true.
    void apply(AssignmentSolution& sol) const {
        sol.unassign(best_.from_emp, best_.day);
        sol.assign(best_.to_emp, best_.day, best_.shift_type);
    }

    /// Evaluate the cost delta of moving from_emp's shift to to_emp on day.
    ///
    /// Temporarily applies the move and reverts it.
    [[nodiscard]] static int evaluate(AssignmentSolution const& sol, int from_emp, int to_emp,
                                      int day) {
        auto& mut = const_cast<AssignmentSolution&>(sol);
        int st = sol.get(from_emp, day);

        int delta = 0;
        delta += mut.unassign(from_emp, day);
        delta += mut.assign(to_emp, day, st);

        // Undo.
        mut.unassign(to_emp, day);
        mut.assign(from_emp, day, st);

        return delta;
    }

    /// The best move found.
    [[nodiscard]] Move const& best_move() const noexcept { return best_; }

    /// The cost delta of the best move found.
    [[nodiscard]] int best_delta() const noexcept { return best_.delta; }

    /// Enumerate all valid shift-move operations.
    [[nodiscard]] static std::vector<Move> enumerate(AssignmentSolution const& sol) {
        std::vector<Move> moves;
        int const ne = sol.num_employees();
        int const H = sol.horizon();

        for (int d = 0; d < H; ++d) {
            for (int from = 0; from < ne; ++from) {
                int st = sol.get(from, d);
                if (st < 0) {
                    continue;
                }

                for (int to = 0; to < ne; ++to) {
                    if (to == from) {
                        continue;
                    }
                    if (sol.get(to, d) >= 0) {
                        continue;
                    }

                    int delta = evaluate(sol, from, to, d);
                    moves.push_back(Move{from, to, d, st, delta});
                }
            }
        }
        return moves;
    }

private:
    Move best_;
};

}  // namespace coso
