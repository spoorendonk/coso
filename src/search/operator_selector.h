#pragma once

#include <cmath>
#include <limits>
#include <vector>

namespace coso {

/// Adaptive operator selector using Upper Confidence Bound (UCB1).
///
/// Multi-armed bandit that learns which search operators are most effective
/// and balances exploitation (best average reward) with exploration (under-
/// tried operators).
///
/// UCB1 formula:  score_i = avg_reward_i + C * sqrt(ln(N) / n_i)
///   - avg_reward_i = total_reward_i / n_i (average improvement)
///   - N = total selections across all operators
///   - n_i = times operator i was selected
///   - C = exploration parameter (default sqrt(2))
///
/// Usage:
///   OperatorSelector selector(num_ops);
///   int op = selector.select();
///   double improvement = try_operator(op);
///   selector.update(op, improvement);
class OperatorSelector {
public:
    /// Construct a selector for the given number of operators.
    ///
    /// @param num_operators  Number of operators to choose from (must be > 0).
    /// @param exploration    UCB1 exploration parameter C (default sqrt(2)).
    explicit OperatorSelector(int num_operators,
                              double exploration = 1.41421356237);

    /// Select the next operator to try (returns index in [0, num_operators)).
    ///
    /// Operators that have never been tried are selected first (round-robin).
    /// After that, the operator with the highest UCB1 score is returned.
    [[nodiscard]] int select() const;

    /// Report the outcome of using operator op_idx.
    ///
    /// @param op_idx  Operator index (must be in [0, num_operators)).
    /// @param reward  Reward signal (typically cost improvement, 0 if none).
    void update(int op_idx, double reward);

    /// Reset all statistics (as if no operators have been tried).
    void reset();

    /// Number of operators.
    [[nodiscard]] int num_operators() const noexcept {
        return static_cast<int>(stats_.size());
    }

    /// Total number of selections across all operators.
    [[nodiscard]] int total_selections() const noexcept {
        return total_selections_;
    }

    /// Number of times operator op_idx has been selected.
    [[nodiscard]] int selections(int op_idx) const noexcept {
        return stats_[op_idx].count;
    }

    /// Total reward accumulated by operator op_idx.
    [[nodiscard]] double total_reward(int op_idx) const noexcept {
        return stats_[op_idx].total_reward;
    }

    /// Number of times operator op_idx produced a positive reward.
    [[nodiscard]] int successes(int op_idx) const noexcept {
        return stats_[op_idx].successes;
    }

    /// Average reward for operator op_idx (0 if never selected).
    [[nodiscard]] double avg_reward(int op_idx) const noexcept {
        if (stats_[op_idx].count == 0)
            return 0.0;
        return stats_[op_idx].total_reward / stats_[op_idx].count;
    }

private:
    struct Stats {
        int count = 0;
        double total_reward = 0.0;
        int successes = 0;
    };

    std::vector<Stats> stats_;
    double exploration_;
    int total_selections_ = 0;
};

} // namespace coso
