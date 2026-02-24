#include "routing/solution.h"

#include <algorithm>

namespace coso {

// ---------------------------------------------------------------------------
//  Construction
// ---------------------------------------------------------------------------

Solution::Solution(ProblemData const& data)
    : data_(&data),
      assigned_(data.num_clients(), false)
{
    // Create one Route per vehicle across all vehicle types.
    int num_vt = data.num_vehicle_types();
    for (int t = 0; t < num_vt; ++t) {
        int count = data.vehicle_type(t).count;
        for (int v = 0; v < count; ++v) {
            routes_.emplace_back(data, t);
        }
    }

    // All clients start unassigned.
    unassigned_.reserve(data.num_clients());
    for (int c = 0; c < data.num_clients(); ++c) {
        unassigned_.push_back(c);
    }
}

// ---------------------------------------------------------------------------
//  Accessors
// ---------------------------------------------------------------------------

int Solution::num_used_vehicles() const noexcept
{
    int count = 0;
    for (auto const& r : routes_) {
        if (!r.empty())
            ++count;
    }
    return count;
}

// ---------------------------------------------------------------------------
//  Cost evaluation
// ---------------------------------------------------------------------------

int64_t Solution::cost(CostEvaluator const& eval) const
{
    int64_t total = 0;
    for (auto const& r : routes_)
        total += eval.route_cost(r);
    return total;
}

int64_t Solution::objective(CostEvaluator const& eval) const
{
    int64_t total = 0;
    for (auto const& r : routes_)
        total += eval.route_objective(r);
    return total;
}

int64_t Solution::penalty(CostEvaluator const& eval) const
{
    int64_t total = 0;
    for (auto const& r : routes_)
        total += eval.route_penalty(r);
    return total;
}

int Solution::total_distance() const noexcept
{
    int total = 0;
    for (auto const& r : routes_)
        total += r.distance();
    return total;
}

bool Solution::feasible() const noexcept
{
    for (auto const& r : routes_) {
        if (!r.load_feasible())
            return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  Modification
// ---------------------------------------------------------------------------

void Solution::set_route_clients(int vehicle, std::vector<int> clients)
{
    assert(vehicle >= 0 && vehicle < num_routes());

    // Unmark old clients in this route (skip DEPOT_VISIT markers).
    auto const& old_route = routes_[vehicle];
    for (int i = 0; i < old_route.size(); ++i) {
        int c = old_route.client(i);
        if (c != DEPOT_VISIT)
            assigned_[c] = false;
    }

    // Mark new clients as assigned (skip DEPOT_VISIT markers).
    for (int c : clients) {
        if (c == DEPOT_VISIT)
            continue;
        assert(c >= 0 && c < data_->num_clients());
        assigned_[c] = true;
    }

    routes_[vehicle].set_clients(std::move(clients));
    rebuild_unassigned_();
}

void Solution::insert_client(int vehicle, int pos, int client)
{
    assert(vehicle >= 0 && vehicle < num_routes());
    assert(client >= 0 && client < data_->num_clients());
    assert(!assigned_[client]);

    routes_[vehicle].insert(pos, client);
    assigned_[client] = true;

    // Remove from unassigned list.
    auto it = std::find(unassigned_.begin(), unassigned_.end(), client);
    assert(it != unassigned_.end());
    // Swap with last for O(1) removal (order doesn't matter).
    std::swap(*it, unassigned_.back());
    unassigned_.pop_back();
}

void Solution::remove_client(int vehicle, int pos)
{
    assert(vehicle >= 0 && vehicle < num_routes());
    assert(pos >= 0 && pos < routes_[vehicle].size());

    int client = routes_[vehicle].client(pos);
    routes_[vehicle].remove(pos);

    // DEPOT_VISIT markers are not real clients -- don't track assignment.
    if (client != DEPOT_VISIT) {
        assigned_[client] = false;
        unassigned_.push_back(client);
    }
}

// ---------------------------------------------------------------------------
//  Internal
// ---------------------------------------------------------------------------

void Solution::rebuild_unassigned_()
{
    unassigned_.clear();
    for (int c = 0; c < data_->num_clients(); ++c) {
        if (!assigned_[c])
            unassigned_.push_back(c);
    }
}

} // namespace coso
