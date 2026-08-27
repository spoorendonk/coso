#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace coso {

/// Resource enforcing synchronized visits across vehicles.
///
/// Certain clients belong to sync groups: pairs of clients that must be
/// visited by different vehicles within a specified time tolerance.  For
/// example, two technicians who must arrive at related sites within 30
/// minutes of each other.
///
/// Within a single route, the resource tracks which sync group members have
/// been visited and their arrival times.  At the solution level, sync
/// feasibility is checked by comparing arrival times of group members across
/// different routes.
///
/// State per subsequence:
///   - entries: for each sync group member visited, the group id and
///     arrival time at that member.
///
/// The excess function compares entries from two routes that share sync
/// group members and penalizes time differences beyond the tolerance.
struct SyncResource {
    /// A sync group: a set of clients that must be visited within a time
    /// tolerance of each other (possibly by different vehicles).
    struct SyncGroup {
        int group_id = 0;          ///< Unique group identifier.
        std::vector<int> clients;  ///< Client indices in this group.
        int tolerance = 0;         ///< Max allowed time difference.
    };

    /// Per-client sync group membership.
    struct ClientInfo {
        int group_id = -1;     ///< Sync group id, or -1 if not in any group.
        int arrival_time = 0;  ///< Placeholder; set during route evaluation.
    };

    /// An entry recording a sync group visit within a subsequence.
    struct Entry {
        int group_id = -1;     ///< Sync group this visit belongs to.
        int client = -1;       ///< Client index.
        int arrival_time = 0;  ///< Arrival time at this client.
    };

    /// State: list of sync group visits in a subsequence.
    struct State {
        std::vector<Entry> entries;
    };

    /// Build a client-to-group lookup table from sync group definitions.
    [[nodiscard]] static std::vector<int> build_lookup(int num_clients,
                                                       std::vector<SyncGroup> const& groups) {
        std::vector<int> lookup(num_clients, -1);
        for (auto const& g : groups) {
            for (int c : g.clients) {
                assert(c >= 0 && c < num_clients);
                lookup[c] = g.group_id;
            }
        }
        return lookup;
    }

    /// Initialize state for a single client node.
    ///
    /// @param group_lookup  Per-client group id (-1 = no group).
    /// @param client        Client index.
    /// @param arrival       Arrival time at this client.
    [[nodiscard]] static State init(std::vector<int> const& group_lookup, int client, int arrival) {
        assert(client >= 0 && client < static_cast<int>(group_lookup.size()));
        State s;
        if (group_lookup[client] >= 0) {
            s.entries.push_back({group_lookup[client], client, arrival});
        }
        return s;
    }

    /// Initialize empty state at depot (no sync visits).
    [[nodiscard]] static State init_depot() { return State{}; }

    /// Merge two adjacent subsequence states.
    ///
    /// Concatenates the entry lists.
    [[nodiscard]] static State merge(State const& left, State const& right) {
        State result;
        result.entries.reserve(left.entries.size() + right.entries.size());
        result.entries.insert(result.entries.end(), left.entries.begin(), left.entries.end());
        result.entries.insert(result.entries.end(), right.entries.begin(), right.entries.end());
        return result;
    }

    /// Merge when the right subsequence is reversed.
    ///
    /// Entry list is the same (just concatenated); individual arrival times
    /// would need recalculation by the caller if direction matters.  For
    /// sync checking, the entries are informational.
    [[nodiscard]] static State merge_reverse(State const& left, State const& right) {
        return merge(left, right);
    }

    /// Compute sync violation between two route states.
    ///
    /// For each sync group that has members in both route_a and route_b,
    /// the excess is the sum of max(0, |arrival_a - arrival_b| - tolerance)
    /// across all such pairs.
    ///
    /// @param route_a   State from one route.
    /// @param route_b   State from another route.
    /// @param groups    The sync group definitions (for tolerance lookup).
    [[nodiscard]] static int excess(State const& route_a, State const& route_b,
                                    std::vector<SyncGroup> const& groups) {
        // Build group_id -> tolerance lookup.
        std::unordered_map<int, int> tol_map;
        for (auto const& g : groups) {
            tol_map[g.group_id] = g.tolerance;
        }

        // Index route_b entries by group_id.
        std::unordered_map<int, std::vector<Entry const*>> b_by_group;
        for (auto const& e : route_b.entries) {
            b_by_group[e.group_id].push_back(&e);
        }

        int total = 0;
        for (auto const& ea : route_a.entries) {
            auto it = b_by_group.find(ea.group_id);
            if (it == b_by_group.end()) {
                continue;
            }

            int tol = 0;
            auto tit = tol_map.find(ea.group_id);
            if (tit != tol_map.end()) {
                tol = tit->second;
            }

            for (auto const* eb : it->second) {
                int diff = std::abs(ea.arrival_time - eb->arrival_time);
                if (diff > tol) {
                    total += diff - tol;
                }
            }
        }

        return total;
    }

    /// Single-route excess: always 0 (sync is inter-route).
    [[nodiscard]] static int excess(State const& /*state*/) { return 0; }
};

}  // namespace coso
