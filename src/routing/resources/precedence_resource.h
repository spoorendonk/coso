#pragma once

#include "routing/problem_data.h"

#include <algorithm>
#include <cassert>
#include <vector>

namespace coso {

/// Resource enforcing pickup-before-delivery precedence within a route.
///
/// For each pickup-delivery request, the pickup client must appear before
/// the delivery client in the same route.  The state tracks:
///   - active: sorted set of request indices whose pickup has been seen but
///             not yet matched by a delivery ("open" pickups).
///   - needed: sorted set of request indices whose delivery has been seen but
///             whose pickup has not yet appeared from the left.
///   - excess: accumulated violation count from inner merges (always 0 for
///             this resource, since all violations surface via needed).
///
/// Designed for O(1)-style merge of adjacent subsequences via the
/// concatenation framework used by Route.
struct PrecedenceResource {
    /// State for a subsequence.
    struct State {
        /// Request indices whose pickup is in this subsequence but whose
        /// delivery is not (yet).
        std::vector<int> active;

        /// Request indices whose delivery is in this subsequence but whose
        /// pickup is not — need a pickup from a left neighbour.
        std::vector<int> needed;

        /// Accumulated excess from inner merges.
        int excess = 0;
    };

    /// Per-client role in pickup-delivery requests.
    struct ClientInfo {
        int request = -1;   ///< Request index, or -1 if not part of any request.
        bool is_pickup = false;
        bool is_delivery = false;
    };

    /// Build a client-to-request lookup table.  Call once per ProblemData.
    [[nodiscard]] static std::vector<ClientInfo>
    build_lookup(ProblemData const& data) {
        std::vector<ClientInfo> lookup(data.num_clients());
        auto const& reqs = data.requests();
        for (int r = 0; r < static_cast<int>(reqs.size()); ++r) {
            auto const& req = reqs[r];
            assert(req.pickup >= 0 && req.pickup < data.num_clients());
            assert(req.delivery >= 0 && req.delivery < data.num_clients());
            lookup[req.pickup].request = r;
            lookup[req.pickup].is_pickup = true;
            lookup[req.delivery].request = r;
            lookup[req.delivery].is_delivery = true;
        }
        return lookup;
    }

    /// Initialize state for a single client node.
    [[nodiscard]] static State init(std::vector<ClientInfo> const& lookup,
                                    int client) {
        State s;
        auto const& info = lookup[client];
        if (info.request < 0)
            return s;

        if (info.is_pickup)
            s.active.push_back(info.request);
        if (info.is_delivery)
            s.needed.push_back(info.request);
        return s;
    }

    /// Initialize empty state at depot.
    [[nodiscard]] static State init_depot() {
        return State{};
    }

    /// Merge two adjacent subsequence states (left followed by right).
    ///
    /// result.needed = left.needed + (right.needed \ left.active)
    /// result.active = (left.active \ right.needed) + right.active
    [[nodiscard]] static State merge(State const& left, State const& right) {
        State result;
        result.excess = left.excess + right.excess;

        // Requests resolved at the junction: right needs pickup, left has it.
        std::vector<int> resolved;
        std::set_intersection(right.needed.begin(), right.needed.end(),
                              left.active.begin(), left.active.end(),
                              std::back_inserter(resolved));

        // Propagate unresolved right deliveries + all left deliveries.
        std::vector<int> right_unresolved;
        std::set_difference(right.needed.begin(), right.needed.end(),
                            resolved.begin(), resolved.end(),
                            std::back_inserter(right_unresolved));
        std::merge(left.needed.begin(), left.needed.end(),
                   right_unresolved.begin(), right_unresolved.end(),
                   std::back_inserter(result.needed));

        // Propagate left pickups not consumed by right + all right pickups.
        std::vector<int> left_remaining;
        std::set_difference(left.active.begin(), left.active.end(),
                            resolved.begin(), resolved.end(),
                            std::back_inserter(left_remaining));
        std::merge(left_remaining.begin(), left_remaining.end(),
                   right.active.begin(), right.active.end(),
                   std::back_inserter(result.active));

        return result;
    }

    /// Compute precedence excess for a (possibly partial) route state.
    ///
    /// Returns the number of unresolved deliveries (those that needed
    /// a pickup from the left but never got one) plus any accumulated excess.
    [[nodiscard]] static int excess(State const& state) {
        return state.excess + static_cast<int>(state.needed.size());
    }
};

} // namespace coso
