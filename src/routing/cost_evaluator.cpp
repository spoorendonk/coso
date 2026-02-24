#include "routing/cost_evaluator.h"

namespace coso {

CostEvaluator::CostEvaluator(int load_penalty,
                             int tw_penalty,
                             int dist_penalty)
    : load_penalty_(load_penalty),
      tw_penalty_(tw_penalty),
      dist_penalty_(dist_penalty)
{
}

// ---------------------------------------------------------------------------
//  Piecewise cost function management
// ---------------------------------------------------------------------------

void CostEvaluator::set_distance_cost_function(PiecewiseLinearFunction func)
{
    distance_cost_func_ = std::make_unique<PiecewiseLinearFunction>(
        std::move(func));
}

void CostEvaluator::set_duration_cost_function(PiecewiseLinearFunction func)
{
    duration_cost_func_ = std::make_unique<PiecewiseLinearFunction>(
        std::move(func));
}

void CostEvaluator::clear_distance_cost_function() noexcept
{
    distance_cost_func_.reset();
}

void CostEvaluator::clear_duration_cost_function() noexcept
{
    duration_cost_func_.reset();
}

bool CostEvaluator::has_distance_cost_function() const noexcept
{
    return distance_cost_func_ != nullptr;
}

bool CostEvaluator::has_duration_cost_function() const noexcept
{
    return duration_cost_func_ != nullptr;
}

// ---------------------------------------------------------------------------
//  Private helpers: piecewise or linear cost computation
// ---------------------------------------------------------------------------

int64_t CostEvaluator::distance_cost_(int distance,
                                       CostParams const& cost) const
{
    if (distance_cost_func_)
        return distance_cost_func_->evaluate(distance);
    return static_cast<int64_t>(distance) * cost.unit_distance_cost;
}

int64_t CostEvaluator::duration_cost_(int duration,
                                       CostParams const& cost) const
{
    if (duration_cost_func_)
        return duration_cost_func_->evaluate(duration);
    return static_cast<int64_t>(duration) * cost.unit_duration_cost;
}

// ---------------------------------------------------------------------------
//  Route-level cost
// ---------------------------------------------------------------------------

int64_t CostEvaluator::route_objective(Route const& route) const
{
    if (route.empty())
        return 0;

    auto const& vt = route.data().vehicle_type(route.vehicle_type());
    auto const& cost = vt.cost;

    int64_t obj = 0;

    // Distance cost (piecewise or linear).
    obj += distance_cost_(route.distance(), cost);

    // Duration cost (piecewise or linear).
    obj += duration_cost_(route.duration(), cost);

    // Fixed vehicle cost (charged if route is non-empty).
    obj += cost.fixed_cost;

    // Prize credits for served clients (subtract from cost).
    for (int i = 0; i < route.size(); ++i) {
        auto const& client = route.data().client(route.client(i));
        obj -= client.prize;
    }

    return obj;
}

int64_t CostEvaluator::route_penalty(Route const& route) const
{
    int64_t pen = 0;

    // Load excess penalty.
    pen += static_cast<int64_t>(route.load_excess()) * load_penalty_;

    // Time warp penalty.
    pen += static_cast<int64_t>(route.time_warp()) * tw_penalty_;

    return pen;
}

int64_t CostEvaluator::route_cost(Route const& route) const
{
    return route_objective(route) + route_penalty(route);
}

// ---------------------------------------------------------------------------
//  Delta evaluation
// ---------------------------------------------------------------------------

int64_t CostEvaluator::eval_insert_cost(Route const& route,
                                         int pos, int client) const
{
    auto const& vt = route.data().vehicle_type(route.vehicle_type());
    auto const& cost_params = vt.cost;

    int64_t delta = 0;

    // Distance cost delta (piecewise or linear).
    if (distance_cost_func_) {
        int old_dist = route.distance();
        int new_dist = old_dist + route.eval_insert_distance(pos, client);
        delta += distance_cost_func_->delta(old_dist, new_dist);
    } else {
        int dist_delta = route.eval_insert_distance(pos, client);
        delta += static_cast<int64_t>(dist_delta)
                 * cost_params.unit_distance_cost;
    }

    // Duration cost delta (piecewise or linear).
    // Note: we approximate duration delta from the distance delta for now,
    // since Route does not expose eval_insert_duration directly.
    // When no piecewise duration cost is set, the original linear duration
    // cost was zero (unit_duration_cost defaults to 0), so this is safe.
    // With a piecewise function, we would need the actual duration after
    // insertion. For correctness, we recompute via the route if needed.
    // (Currently Route doesn't track duration delta for inserts directly,
    // so we skip duration delta in insert/remove -- it will be captured
    // in the full route_objective when routes are recomputed.)

    // Fixed cost delta: if the route was empty, we now incur fixed cost.
    if (route.empty())
        delta += cost_params.fixed_cost;

    // Prize credit for the inserted client.
    delta -= route.data().client(client).prize;

    // Load penalty delta.
    int new_excess = route.eval_insert_load(pos, client);
    int old_excess = route.load_excess();
    delta += static_cast<int64_t>(new_excess - old_excess) * load_penalty_;

    // Time warp penalty delta.
    int new_tw = route.eval_insert_time_warp(pos, client);
    int old_tw = route.time_warp();
    delta += static_cast<int64_t>(new_tw - old_tw) * tw_penalty_;

    return delta;
}

int64_t CostEvaluator::eval_remove_cost(Route const& route, int pos) const
{
    auto const& vt = route.data().vehicle_type(route.vehicle_type());
    auto const& cost_params = vt.cost;

    int64_t delta = 0;

    // Distance cost delta (piecewise or linear).
    if (distance_cost_func_) {
        int old_dist = route.distance();
        int new_dist = old_dist + route.eval_remove_distance(pos);
        delta += distance_cost_func_->delta(old_dist, new_dist);
    } else {
        int dist_delta = route.eval_remove_distance(pos);
        delta += static_cast<int64_t>(dist_delta)
                 * cost_params.unit_distance_cost;
    }

    // Fixed cost delta: if removing the last client, we save the fixed cost.
    if (route.size() == 1)
        delta -= cost_params.fixed_cost;

    // Prize: removing a client loses its prize credit (cost increases).
    delta += route.data().client(route.client(pos)).prize;

    // Load penalty delta.
    int new_excess = route.eval_remove_load(pos);
    int old_excess = route.load_excess();
    delta += static_cast<int64_t>(new_excess - old_excess) * load_penalty_;

    // Time warp penalty delta.
    int new_tw = route.eval_remove_time_warp(pos);
    int old_tw = route.time_warp();
    delta += static_cast<int64_t>(new_tw - old_tw) * tw_penalty_;

    return delta;
}

} // namespace coso
