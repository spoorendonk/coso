#pragma once

#include <algorithm>
#include <cassert>
#include <vector>

namespace coso {

/// Resource enforcing LIFO/FIFO loading constraints on a route.
///
/// In many real-world routing problems, items are physically stacked in the
/// vehicle and must be unloaded in a specific order:
///
///   - LIFO (Last-In-First-Out): Items loaded last must be unloaded first.
///     This is the "stack" constraint — common for single-door vehicles.
///
///   - FIFO (First-In-First-Out): Items loaded first are unloaded first.
///     This is the "queue" constraint — for conveyor-style or drive-through.
///
/// Each client is either a pickup (loading) or a delivery (unloading).
/// The resource tracks the loading sequence and checks that deliveries
/// respect the required order.
///
/// For LIFO: if pickup A is loaded before pickup B, then B must be delivered
/// before A (i.e., the delivery order is the reverse of the pickup order).
///
/// For FIFO: if pickup A is loaded before pickup B, then A must be delivered
/// before B (i.e., delivery order matches pickup order).
///
/// State tracks:
///   - pending: ordered list of request ids whose pickup has been seen but
///     delivery has not.  This is the "stack" (LIFO) or "queue" (FIFO).
///   - violations: count of ordering violations detected within this subseq.
///
/// Designed for O(n) merge where n is the number of pending items.
struct LoadingResource {
    /// Loading policy.
    enum class Policy { LIFO, FIFO };

    /// Per-client role in loading requests.
    struct ClientInfo {
        int request = -1;       ///< Request index, or -1 if not part of any.
        bool is_pickup = false;
        bool is_delivery = false;
    };

    /// Build a client-to-request lookup table.
    ///
    /// @param num_clients   Total number of clients.
    /// @param pickups       pickups[r] = client index of request r's pickup.
    /// @param deliveries    deliveries[r] = client index of request r's delivery.
    [[nodiscard]] static std::vector<ClientInfo>
    build_lookup(int num_clients,
                 std::vector<int> const& pickups,
                 std::vector<int> const& deliveries)
    {
        assert(pickups.size() == deliveries.size());
        int nr = static_cast<int>(pickups.size());

        std::vector<ClientInfo> lookup(num_clients);
        for (int r = 0; r < nr; ++r) {
            assert(pickups[r] >= 0 && pickups[r] < num_clients);
            assert(deliveries[r] >= 0 && deliveries[r] < num_clients);
            lookup[pickups[r]].request = r;
            lookup[pickups[r]].is_pickup = true;
            lookup[deliveries[r]].request = r;
            lookup[deliveries[r]].is_delivery = true;
        }
        return lookup;
    }

    /// State for a subsequence.
    struct State {
        /// Ordered list of request ids whose pickup is in this subsequence
        /// but whose delivery is not.  Order reflects visit order.
        std::vector<int> pending;

        /// Request ids whose delivery is in this subsequence but whose
        /// pickup is not (need a pickup from the left).
        std::vector<int> needed;

        /// Position within needed that each delivery was encountered
        /// (for ordering checks during merge).  Same length as needed.
        /// Not used directly; ordering is implicit from the vector order.

        /// Count of LIFO/FIFO violations within this subsequence.
        int violations = 0;
    };

    /// Initialize state for a single client node.
    [[nodiscard]] static State init(std::vector<ClientInfo> const& lookup,
                                    int client) {
        assert(client >= 0
               && client < static_cast<int>(lookup.size()));
        State s;
        auto const& info = lookup[client];
        if (info.request < 0)
            return s;

        if (info.is_pickup)
            s.pending.push_back(info.request);
        if (info.is_delivery)
            s.needed.push_back(info.request);
        return s;
    }

    /// Initialize empty state at depot.
    [[nodiscard]] static State init_depot() {
        return State{};
    }

    /// Merge two adjacent subsequence states.
    ///
    /// When merging left and right:
    /// 1. Some of right.needed may be satisfied by left.pending.
    /// 2. The order in which those deliveries appear (from right.needed)
    ///    must match the required order from left.pending.
    ///
    /// @param left    Left (earlier) subsequence state.
    /// @param right   Right (later) subsequence state.
    /// @param policy  LIFO or FIFO.
    [[nodiscard]] static State merge(State const& left, State const& right,
                                     Policy policy) {
        State result;
        result.violations = left.violations + right.violations;

        // Determine which of right's needed deliveries are satisfied by
        // left's pending pickups.
        // right.needed is in the order deliveries appear in the right subseq.
        // We need to check if this delivery order is consistent with the
        // pickup order in left.pending.

        // Collect the requests that are resolved at this junction.
        // These are requests in both left.pending and right.needed.
        // We preserve their order from right.needed (delivery order).
        std::vector<int> resolved_delivery_order;
        {
            // Build set of left.pending for O(1) lookup.
            // Using a simple scan since lists are typically small.
            auto in_pending = [&](int req) {
                return std::find(left.pending.begin(), left.pending.end(), req)
                       != left.pending.end();
            };
            for (int req : right.needed) {
                if (in_pending(req))
                    resolved_delivery_order.push_back(req);
            }
        }

        // Extract the pickup order of resolved requests from left.pending.
        std::vector<int> resolved_pickup_order;
        {
            auto in_resolved = [&](int req) {
                return std::find(resolved_delivery_order.begin(),
                                 resolved_delivery_order.end(), req)
                       != resolved_delivery_order.end();
            };
            for (int req : left.pending) {
                if (in_resolved(req))
                    resolved_pickup_order.push_back(req);
            }
        }

        // Check ordering constraint.
        // LIFO: delivery order should be reverse of pickup order.
        // FIFO: delivery order should match pickup order.
        if (resolved_pickup_order.size() >= 2) {
            std::vector<int> expected;
            if (policy == Policy::LIFO) {
                expected.assign(resolved_pickup_order.rbegin(),
                                resolved_pickup_order.rend());
            } else {
                expected = resolved_pickup_order;
            }

            // Count violations: number of positions where delivery order
            // disagrees with expected order.
            for (size_t i = 0; i < expected.size(); ++i) {
                if (resolved_delivery_order[i] != expected[i])
                    ++result.violations;
            }
        }

        // Propagate unresolved right.needed + all left.needed.
        for (int req : left.needed)
            result.needed.push_back(req);
        for (int req : right.needed) {
            if (std::find(resolved_delivery_order.begin(),
                          resolved_delivery_order.end(), req)
                == resolved_delivery_order.end())
                result.needed.push_back(req);
        }

        // Propagate left.pending not consumed + all right.pending.
        for (int req : left.pending) {
            if (std::find(resolved_pickup_order.begin(),
                          resolved_pickup_order.end(), req)
                == resolved_pickup_order.end())
                result.pending.push_back(req);
        }
        for (int req : right.pending)
            result.pending.push_back(req);

        return result;
    }

    /// Compute loading order excess for a route state.
    ///
    /// Returns the number of ordering violations plus the number of
    /// unresolved deliveries (those that need a pickup from the left).
    [[nodiscard]] static int excess(State const& state) {
        return state.violations + static_cast<int>(state.needed.size());
    }
};

} // namespace coso
