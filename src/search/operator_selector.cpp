#include "search/operator_selector.h"

#include <cassert>

namespace coso {

OperatorSelector::OperatorSelector(int num_operators, double exploration)
    : stats_(num_operators), exploration_(exploration)
{
    assert(num_operators > 0);
}

int OperatorSelector::select() const
{
    assert(!stats_.empty());

    // Phase 1: try each operator at least once (round-robin).
    for (int i = 0; i < static_cast<int>(stats_.size()); ++i) {
        if (stats_[i].count == 0)
            return i;
    }

    // Phase 2: UCB1 selection.
    double log_n = std::log(static_cast<double>(total_selections_));
    double best_score = -std::numeric_limits<double>::infinity();
    int best_idx = 0;

    for (int i = 0; i < static_cast<int>(stats_.size()); ++i) {
        double avg = stats_[i].total_reward / stats_[i].count;
        double explore = exploration_ * std::sqrt(log_n / stats_[i].count);
        double score = avg + explore;

        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }

    return best_idx;
}

void OperatorSelector::update(int op_idx, double reward)
{
    assert(op_idx >= 0 && op_idx < static_cast<int>(stats_.size()));

    auto& s = stats_[op_idx];
    s.count++;
    s.total_reward += reward;
    if (reward > 0.0)
        s.successes++;
    total_selections_++;
}

void OperatorSelector::reset()
{
    for (auto& s : stats_) {
        s.count = 0;
        s.total_reward = 0.0;
        s.successes = 0;
    }
    total_selections_ = 0;
}

} // namespace coso
