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
//  Route-level cost
// ---------------------------------------------------------------------------

int64_t CostEvaluator::route_objective(Route const& route) const
{
    if (route.empty())
        return 0;

    auto const& vt = route.data().vehicle_type(route.vehicle_type());
    auto const& cost = vt.cost;

    int64_t obj = 0;

    // Distance cost.
    obj += static_cast<int64_t>(route.distance()) * cost.unit_distance_cost;

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

    // Distance delta.
    int dist_delta = route.eval_insert_distance(pos, client);
    delta += static_cast<int64_t>(dist_delta) * cost_params.unit_distance_cost;

    // Fixed cost delta: if the route was empty, we now incur fixed cost.
    if (route.empty())
        delta += cost_params.fixed_cost;

    // Prize credit for the inserted client.
    delta -= route.data().client(client).prize;

    // Load penalty delta.
    int new_excess = route.eval_insert_load(pos, client);
    int old_excess = route.load_excess();
    delta += static_cast<int64_t>(new_excess - old_excess) * load_penalty_;

    return delta;
}

int64_t CostEvaluator::eval_remove_cost(Route const& route, int pos) const
{
    auto const& vt = route.data().vehicle_type(route.vehicle_type());
    auto const& cost_params = vt.cost;

    int64_t delta = 0;

    // Distance delta.
    int dist_delta = route.eval_remove_distance(pos);
    delta += static_cast<int64_t>(dist_delta) * cost_params.unit_distance_cost;

    // Fixed cost delta: if removing the last client, we save the fixed cost.
    if (route.size() == 1)
        delta -= cost_params.fixed_cost;

    // Prize: removing a client loses its prize credit (cost increases).
    delta += route.data().client(route.client(pos)).prize;

    // Load penalty delta.
    int new_excess = route.eval_remove_load(pos);
    int old_excess = route.load_excess();
    delta += static_cast<int64_t>(new_excess - old_excess) * load_penalty_;

    return delta;
}

} // namespace coso
