#include "routing/cost_evaluator.h"

namespace coso {

CostEvaluator::CostEvaluator(int load_penalty, int tw_penalty, int dist_penalty)
    : load_penalty_(load_penalty), tw_penalty_(tw_penalty), dist_penalty_(dist_penalty) {}

// ---------------------------------------------------------------------------
//  Piecewise cost function management
// ---------------------------------------------------------------------------

void CostEvaluator::set_distance_cost_function(PiecewiseLinearFunction func) {
    distance_cost_func_ = std::make_unique<PiecewiseLinearFunction>(std::move(func));
}

void CostEvaluator::set_duration_cost_function(PiecewiseLinearFunction func) {
    duration_cost_func_ = std::make_unique<PiecewiseLinearFunction>(std::move(func));
}

void CostEvaluator::clear_distance_cost_function() noexcept {
    distance_cost_func_.reset();
}

void CostEvaluator::clear_duration_cost_function() noexcept {
    duration_cost_func_.reset();
}

bool CostEvaluator::has_distance_cost_function() const noexcept {
    return distance_cost_func_ != nullptr;
}

bool CostEvaluator::has_duration_cost_function() const noexcept {
    return duration_cost_func_ != nullptr;
}

// ---------------------------------------------------------------------------
//  Private helpers: piecewise or plain cost computation
// ---------------------------------------------------------------------------

int64_t CostEvaluator::distance_cost_(int distance) const {
    if (distance_cost_func_) {
        return distance_cost_func_->evaluate(distance);
    }
    return distance;
}

int64_t CostEvaluator::duration_cost_(int duration) const {
    if (duration_cost_func_) {
        return duration_cost_func_->evaluate(duration);
    }
    return 0;
}

// ---------------------------------------------------------------------------
//  Route-level cost
// ---------------------------------------------------------------------------

int64_t CostEvaluator::route_objective(Route const& route) const {
    if (route.empty()) {
        return 0;
    }

    int64_t obj = 0;

    // Distance cost (piecewise or plain).
    obj += distance_cost_(route.distance());

    // Duration cost (piecewise, zero otherwise).
    obj += duration_cost_(route.duration());

    // Prize credits for served clients (subtract from cost).
    for (int i = 0; i < route.size(); ++i) {
        auto const& client = route.data().client(route.client(i));
        obj -= client.prize;
    }

    return obj;
}

int64_t CostEvaluator::route_penalty(Route const& route) const {
    int64_t pen = 0;

    // Load excess penalty.
    pen += static_cast<int64_t>(route.load_excess()) * load_penalty_;

    // Time warp penalty.
    pen += static_cast<int64_t>(route.time_warp()) * tw_penalty_;

    return pen;
}

int64_t CostEvaluator::route_cost(Route const& route) const {
    return route_objective(route) + route_penalty(route);
}

// ---------------------------------------------------------------------------
//  Delta evaluation
// ---------------------------------------------------------------------------

int64_t CostEvaluator::eval_insert_cost(Route const& route, int pos, int client) const {
    return eval_splice_cost(route, pos, pos, std::span<int const>(&client, 1));
}

int64_t CostEvaluator::eval_remove_cost(Route const& route, int pos) const {
    return eval_splice_cost(route, pos, pos + 1, {});
}

int64_t CostEvaluator::eval_splice_cost(Route const& route, int first, int last,
                                        std::span<int const> seq) const {
    auto after = route.eval_splice(first, last, seq);

    // Distance and duration terms, which route_objective charges only on a
    // non-empty route.
    int64_t delta = 0;
    if (after.size > 0) {
        delta += distance_cost_(after.distance) + duration_cost_(after.duration);
    }
    if (!route.empty()) {
        delta -= distance_cost_(route.distance()) + duration_cost_(route.duration());
    }

    // Prize credits: lost for the clients taken out, gained for those put in.
    for (int i = first; i < last; ++i) {
        delta += route.data().client(route.client(i)).prize;
    }
    for (int client : seq) {
        delta -= route.data().client(client).prize;
    }

    delta += static_cast<int64_t>(after.load_excess - route.load_excess()) * load_penalty_;
    delta += static_cast<int64_t>(after.time_warp - route.time_warp()) * tw_penalty_;

    return delta;
}

}  // namespace coso
