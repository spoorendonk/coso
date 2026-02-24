#include "routing/operators/route_split.h"

#include <cassert>
#include <vector>

namespace coso {

// ===========================================================================
//  RouteSplit — split a route into two at the optimal point
// ===========================================================================

bool RouteSplit::find_best_move(Solution const& sol,
                                CostEvaluator const& eval,
                                ProblemData const& data)
{
    best_delta_ = 0;
    source_route_ = -1;

    for (int r = 0; r < sol.num_routes(); ++r) {
        auto const& route = sol.route(r);

        // Need at least 2 clients to split.
        if (route.size() < 2)
            continue;

        int vtype = route.vehicle_type();

        // Find an empty vehicle of the same type to receive the second half.
        int empty_vehicle = -1;
        for (int v = 0; v < sol.num_routes(); ++v) {
            if (v == r)
                continue;
            if (sol.route(v).empty()
                && sol.route(v).vehicle_type() == vtype)
            {
                empty_vehicle = v;
                break;
            }
        }

        if (empty_vehicle < 0)
            continue;  // no available vehicle slot

        int64_t old_cost = eval.route_cost(route);
        // The empty route has zero cost, so old total is just old_cost.

        // Try every split position: split after position p means
        // first half = [0..p], second half = [p+1..size-1].
        for (int p = 0; p + 1 < route.size(); ++p) {
            // Build first half.
            std::vector<int> first_half;
            first_half.reserve(p + 1);
            for (int i = 0; i <= p; ++i)
                first_half.push_back(route.client(i));

            // Build second half.
            std::vector<int> second_half;
            second_half.reserve(route.size() - p - 1);
            for (int i = p + 1; i < route.size(); ++i)
                second_half.push_back(route.client(i));

            // Evaluate the two new routes.
            Route temp_a(data, vtype);
            temp_a.set_clients(std::move(first_half));
            Route temp_b(data, vtype);
            temp_b.set_clients(std::move(second_half));

            int64_t new_cost = eval.route_cost(temp_a) + eval.route_cost(temp_b);
            int64_t delta = new_cost - old_cost;

            if (delta < best_delta_) {
                best_delta_ = delta;
                source_route_ = r;
                split_pos_ = p;
                target_route_ = empty_vehicle;
            }
        }
    }

    return best_delta_ < 0;
}

void RouteSplit::apply(Solution& sol) const
{
    assert(source_route_ >= 0);
    assert(target_route_ >= 0);
    assert(sol.route(target_route_).empty());

    auto const& route = sol.route(source_route_);

    // Build the two halves.
    std::vector<int> first_half;
    first_half.reserve(split_pos_ + 1);
    for (int i = 0; i <= split_pos_; ++i)
        first_half.push_back(route.client(i));

    std::vector<int> second_half;
    second_half.reserve(route.size() - split_pos_ - 1);
    for (int i = split_pos_ + 1; i < route.size(); ++i)
        second_half.push_back(route.client(i));

    // Clear the source route, then assign both halves.
    sol.set_route_clients(source_route_, {});
    sol.set_route_clients(source_route_, std::move(first_half));
    sol.set_route_clients(target_route_, std::move(second_half));
}

} // namespace coso
