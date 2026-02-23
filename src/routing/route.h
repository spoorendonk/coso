#pragma once

#include "routing/problem_data.h"
#include "routing/resources/load_resource.h"

#include <cassert>
#include <span>
#include <vector>

namespace coso {

/// A route: an ordered sequence of clients served by one vehicle.
///
/// Maintains prefix and suffix resource arrays for O(1) move evaluation.
/// Each prefix[i] holds the cumulative resource state for clients 0..i,
/// each suffix[i] holds the cumulative state for clients i..n-1.
///
/// Move evaluation: to evaluate inserting/removing/swapping clients, merge
/// the appropriate prefix and suffix states in O(1) instead of re-scanning
/// the route.
///
/// Node numbering: the route stores client indices (0-based among clients).
/// Conversion to full node indices (with depot offset) is the caller's
/// responsibility.
class Route {
public:
    /// Construct an empty route for the given vehicle type and problem data.
    Route(ProblemData const& data, int vehicle_type);

    // -------------------------------------------------------------------
    //  Accessors
    // -------------------------------------------------------------------

    /// The problem data this route belongs to.
    [[nodiscard]] ProblemData const& data() const noexcept { return *data_; }

    /// Vehicle type index for this route.
    [[nodiscard]] int vehicle_type() const noexcept { return vehicle_type_; }

    /// Number of clients currently in the route.
    [[nodiscard]] int size() const noexcept {
        return static_cast<int>(clients_.size());
    }

    /// Whether the route has no clients.
    [[nodiscard]] bool empty() const noexcept { return clients_.empty(); }

    /// Client index at position pos (0-based among clients).
    [[nodiscard]] int client(int pos) const {
        assert(pos >= 0 && pos < size());
        return clients_[pos];
    }

    /// Read-only access to the full client sequence.
    [[nodiscard]] std::span<int const> clients() const noexcept {
        return clients_;
    }

    /// Load resource prefix state at position pos.
    /// prefix(i) is the merged state for clients [0..i].
    /// prefix(-1) is the empty depot state (before any client).
    [[nodiscard]] LoadResource::State const& load_prefix(int pos) const {
        assert(pos >= -1 && pos < size());
        return load_prefix_[pos + 1];  // index 0 = depot, 1 = first client
    }

    /// Load resource suffix state at position pos.
    /// suffix(i) is the merged state for clients [i..n-1].
    /// suffix(n) is the empty depot state (after all clients).
    [[nodiscard]] LoadResource::State const& load_suffix(int pos) const {
        assert(pos >= 0 && pos <= size());
        return load_suffix_[pos];
    }

    /// Total load excess for this route (sum across all dimensions).
    [[nodiscard]] int load_excess() const noexcept { return load_excess_; }

    /// Whether the route is load-feasible (no capacity violations).
    [[nodiscard]] bool load_feasible() const noexcept {
        return load_excess_ == 0;
    }

    /// Total distance of this route (depot -> clients -> depot).
    [[nodiscard]] int distance() const noexcept { return distance_; }

    // -------------------------------------------------------------------
    //  Modification + resource update
    // -------------------------------------------------------------------

    /// Set the entire client sequence and recompute all resources.
    void set_clients(std::vector<int> clients);

    /// Insert a client at the given position (0 = before first client).
    /// Recomputes prefix/suffix arrays.
    void insert(int pos, int client);

    /// Remove the client at the given position.
    /// Recomputes prefix/suffix arrays.
    void remove(int pos);

    /// Replace the client at the given position with a new client.
    /// Recomputes prefix/suffix arrays.
    void replace(int pos, int new_client);

    // -------------------------------------------------------------------
    //  O(1) move evaluation helpers
    // -------------------------------------------------------------------

    /// Evaluate the load excess if a client were inserted at position pos.
    /// Does NOT modify the route. O(1) using prefix/suffix arrays.
    ///
    /// @param pos     Position to insert at (0..size()).
    /// @param client  Client index to insert.
    /// @return Load excess of the modified route.
    [[nodiscard]] int eval_insert_load(int pos, int client) const;

    /// Evaluate the load excess if the client at position pos were removed.
    /// Does NOT modify the route. O(1) using prefix/suffix arrays.
    [[nodiscard]] int eval_remove_load(int pos) const;

    /// Evaluate the distance delta for inserting a client at position pos.
    /// Returns the change in total route distance (can be negative).
    [[nodiscard]] int eval_insert_distance(int pos, int client) const;

    /// Evaluate the distance delta for removing the client at position pos.
    /// Returns the change in total route distance (can be negative).
    [[nodiscard]] int eval_remove_distance(int pos) const;

private:
    ProblemData const* data_;
    int vehicle_type_;
    int depot_ = 0;  // depot index (node 0 by default)

    std::vector<int> clients_;  // client indices (0-based among clients)

    // Prefix/suffix arrays for load resource.
    // prefix_[0] = depot (empty), prefix_[i+1] = merge(prefix_[i], client[i])
    // suffix_[i] = merge(client[i], suffix_[i+1]), suffix_[n] = depot (empty)
    std::vector<LoadResource::State> load_prefix_;
    std::vector<LoadResource::State> load_suffix_;

    int load_excess_ = 0;
    int distance_    = 0;

    /// Recompute all prefix/suffix arrays and cached values.
    void update_();

    /// Convert client index to node index.
    [[nodiscard]] int node_(int client) const {
        return data_->num_depots() + client;
    }
};

} // namespace coso
