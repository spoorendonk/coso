#include "search/warm_start.h"

#include <algorithm>
#include <format>
#include <stdexcept>

namespace coso {

// ---------------------------------------------------------------------------
//  PinSet
// ---------------------------------------------------------------------------

PinSet::PinSet(int num_clients)
    : is_pinned_(num_clients, false)
{
}

void PinSet::pin(int client)
{
    if (client < 0)
        throw std::invalid_argument(
            std::format("PinSet::pin: invalid client index {}", client));

    // Grow if needed.
    if (client >= static_cast<int>(is_pinned_.size()))
        is_pinned_.resize(client + 1, false);

    if (is_pinned_[client])
        return;  // Already pinned.

    is_pinned_[client] = true;

    // Insert into sorted list.
    auto it = std::lower_bound(pinned_.begin(), pinned_.end(), client);
    pinned_.insert(it, client);
}

void PinSet::unpin(int client)
{
    if (client < 0 || client >= static_cast<int>(is_pinned_.size()))
        return;

    if (!is_pinned_[client])
        return;

    is_pinned_[client] = false;

    auto it = std::lower_bound(pinned_.begin(), pinned_.end(), client);
    if (it != pinned_.end() && *it == client)
        pinned_.erase(it);
}

bool PinSet::is_pinned(int client) const
{
    if (client < 0 || client >= static_cast<int>(is_pinned_.size()))
        return false;
    return is_pinned_[client];
}

void PinSet::clear()
{
    std::fill(is_pinned_.begin(), is_pinned_.end(), false);
    pinned_.clear();
}

// ---------------------------------------------------------------------------
//  warm_start
// ---------------------------------------------------------------------------

Solution warm_start(
    std::vector<std::vector<int>> const& routes,
    ProblemData const& data,
    [[maybe_unused]] CostEvaluator const& eval)
{
    int const num_clients = data.num_clients();
    int const total_vehicles = data.total_vehicles();

    // Validate number of routes.
    if (static_cast<int>(routes.size()) > total_vehicles) {
        throw std::invalid_argument(std::format(
            "warm_start: {} routes provided but only {} vehicles available",
            routes.size(), total_vehicles));
    }

    // Track which clients have been assigned (for duplicate detection).
    std::vector<bool> seen(num_clients, false);

    for (int r = 0; r < static_cast<int>(routes.size()); ++r) {
        for (int client : routes[r]) {
            // Validate client index.
            if (client < 0 || client >= num_clients) {
                throw std::invalid_argument(std::format(
                    "warm_start: client index {} out of range [0, {}) "
                    "in route {}",
                    client, num_clients, r));
            }

            // Check for duplicates.
            if (seen[client]) {
                throw std::invalid_argument(std::format(
                    "warm_start: client {} appears more than once "
                    "(duplicate in route {})",
                    client, r));
            }
            seen[client] = true;
        }
    }

    // Check that all required clients are assigned.
    for (int c = 0; c < num_clients; ++c) {
        if (!seen[c] && data.client(c).required) {
            throw std::invalid_argument(std::format(
                "warm_start: required client {} is not assigned to any route",
                c));
        }
    }

    // Build the solution.
    Solution sol(data);

    for (int r = 0; r < static_cast<int>(routes.size()); ++r) {
        if (!routes[r].empty()) {
            sol.set_route_clients(r, routes[r]);
        }
    }

    return sol;
}

} // namespace coso
