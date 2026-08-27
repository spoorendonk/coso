#include "search/penalty_manager.h"

#include <algorithm>
#include <cmath>

namespace coso {

PenaltyManager::PenaltyManager(double target_feasible, double adjustment_rate, int update_interval)
    : target_feasible_(target_feasible),
      adjustment_rate_(adjustment_rate),
      update_interval_(update_interval) {}

void PenaltyManager::register_solution(bool feasible) {
    ++window_count_;
    ++total_;
    if (feasible) {
        ++window_feasible_;
    }

    if (window_count_ >= update_interval_) {
        update_penalties_();
    }
}

CostEvaluator PenaltyManager::cost_evaluator() const {
    return CostEvaluator(static_cast<int>(std::round(load_pen_)),
                         static_cast<int>(std::round(tw_pen_)),
                         static_cast<int>(std::round(dist_pen_)));
}

void PenaltyManager::update_penalties_() {
    double frac = static_cast<double>(window_feasible_) / window_count_;

    // If too many solutions are feasible, decrease penalties to allow more
    // infeasible exploration.  If too few, increase penalties.
    if (frac > target_feasible_) {
        double factor = 1.0 - adjustment_rate_;
        load_pen_ *= factor;
        tw_pen_ *= factor;
        dist_pen_ *= factor;
    } else if (frac < target_feasible_) {
        double factor = 1.0 + adjustment_rate_;
        load_pen_ *= factor;
        tw_pen_ *= factor;
        dist_pen_ *= factor;
    }

    // Clamp to bounds.
    load_pen_ = std::clamp(load_pen_, kMinPenalty, kMaxPenalty);
    tw_pen_ = std::clamp(tw_pen_, kMinPenalty, kMaxPenalty);
    dist_pen_ = std::clamp(dist_pen_, kMinPenalty, kMaxPenalty);

    // Reset window.
    window_count_ = 0;
    window_feasible_ = 0;
}

}  // namespace coso
