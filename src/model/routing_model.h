#pragma once

#include "types.h"

#include <climits>
#include <string>
#include <utility>
#include <vector>

namespace coso {

/// Parameters for a vehicle type.
struct VehicleTypeParams {
    std::vector<int> capacity;   ///< N load dimensions
    int max_duration = 0;        ///< 0 = unlimited
    int max_distance = 0;        ///< 0 = unlimited
    int min_tasks = 0;           ///< 0 = no minimum
    int max_tasks = 0;           ///< 0 = unlimited
    int max_overtime = 0;        ///< allowed overtime beyond max_duration
    int unit_overtime_cost = 0;  ///< cost per unit of overtime
    int reload_depot = -1;       ///< depot id for multi-trip reload (-1 = none)
    int max_reloads = 0;         ///< max number of reloads per shift
    CostParams cost;
    int profile = 0;  ///< distance/duration matrix index
    std::vector<std::string> skills;
};

/// Parameters for a client location.
struct ClientParams {
    std::vector<int> demand;  ///< N load dimensions (matches capacity)
    std::vector<int> pickup;  ///< backhaul pickup quantities
    TimeWindow tw = {0, INT_MAX};
    std::vector<TimeWindow> extra_tw;  ///< additional time windows
    int service = 0;                   ///< service duration
    int release_time = 0;
    int prize = 0;  ///< for optional clients (Team Orienteering)
    bool required = true;
    int group = -1;  ///< client group id (-1 = none)
    std::vector<std::string> skills;
    int client_type = -1;  ///< type id for incompatibility constraints (-1 = none)
};

/// Parameters for a depot location.
struct DepotParams {
    TimeWindow tw = {0, INT_MAX};
};

/// Routing model: declare depots, vehicles, clients, distances, then solve.
///
/// What the native engine enforces is CVRP: demand against an N-dimensional
/// vehicle capacity, minimising distance.  Most of the rest of this schema is
/// accepted and dropped — see the Routing section of docs/models.md for the
/// per-field verdict, and #194, #196 and #198 for the defects behind it.
class RoutingModel {
public:
    // -- Stored entry types --------------------------------------------------
    //
    //  What the model keeps for each declaration, returned by the accessors
    //  below so a backend adapter can read a model back.

    /// A depot as declared: either coordinates or an explicit node id.
    struct DepotEntry {
        double x = 0.0, y = 0.0;
        bool has_coord = false;  ///< false when added with explicit id
        int explicit_id = -1;    ///< node id when added without coordinates
        DepotParams params;
    };

    /// A client as declared.  add_pickup / add_delivery are aliases for
    /// add_client, so the pickup/delivery role is not stored here — only the
    /// pairing recorded by add_request, see requests().
    struct ClientEntry {
        double x = 0.0, y = 0.0;
        bool has_coord = false;
        int explicit_id = -1;
        ClientParams params;
    };

    /// A vehicle type as declared, with its fleet count.
    struct VehicleTypeEntry {
        int count = 0;
        VehicleTypeParams params;
    };

    /// One explicit distance / duration / cost matrix entry.
    struct MatEntry {
        int profile;
        int from;
        int to;
        int value;
    };

    /// Add a depot at the given coordinates.
    int add_depot(double x, double y, DepotParams p = {});

    /// Add a depot with an explicit node id (when using explicit distances).
    int add_depot(int id, DepotParams p = {});

    /// Add a vehicle type with the given count and parameters.
    int add_vehicle_type(int count, VehicleTypeParams p = {});

    /// Add a client at the given coordinates.
    int add_client(double x, double y, ClientParams p = {});

    /// Add a client with an explicit node id (when using explicit distances).
    int add_client(int id, ClientParams p = {});

    /// Add a pickup location (for pickup-delivery problems).
    int add_pickup(double x, double y, ClientParams p = {});

    /// Add a delivery location (for pickup-delivery problems).
    int add_delivery(double x, double y, ClientParams p = {});

    /// Link a pickup-delivery pair: both on the same route, pickup before delivery.
    void add_request(int pickup, int delivery);

    /// Convenience alias for add_request.
    void add_pickup_delivery(int pickup, int delivery);

    /// Create a client group (exactly one member must be served).
    int add_client_group();

    // -- Distance / duration matrices ----------------------------------------

    /// Set the distance between two nodes (default profile 0).
    void set_distance(int from, int to, int dist);

    /// Set the travel duration between two nodes (default profile 0).
    void set_duration(int from, int to, int dur);

    /// Select the active profile for subsequent set_distance / set_duration calls.
    void set_profile(int profile);

    /// Set distance for a specific routing profile.
    void set_profile_distance(int profile, int from, int to, int dist);

    /// Set duration for a specific routing profile.
    void set_profile_duration(int profile, int from, int to, int dur);

    /// Set a cost matrix entry for a specific profile.
    void set_cost_matrix(int profile, int from, int to, int cost);

    // -- Warm start / re-optimization ----------------------------------------

    /// Provide an initial solution (list of routes, each a list of client ids).
    void set_initial_routes(const std::vector<std::vector<int>>& routes);

    /// Pin a client in its current position during re-optimization.
    void pin(int client_id);

    // -- Solve ---------------------------------------------------------------

    /// Solve the routing problem within the given time limit.
    Result solve(TimeLimit tl);

    // -- Accessors -----------------------------------------------------------

    [[nodiscard]] int num_depots() const noexcept { return static_cast<int>(depots_.size()); }
    [[nodiscard]] DepotEntry const& depot(int d) const { return depots_[d]; }

    [[nodiscard]] int num_clients() const noexcept { return static_cast<int>(clients_.size()); }
    [[nodiscard]] ClientEntry const& client(int c) const { return clients_[c]; }

    [[nodiscard]] int num_vehicle_types() const noexcept {
        return static_cast<int>(vehicle_types_.size());
    }
    [[nodiscard]] VehicleTypeEntry const& vehicle_type(int v) const { return vehicle_types_[v]; }

    /// Number of groups handed out by add_client_group().  This does NOT bound
    /// the group ids clients carry: ClientParams::group is never validated, so
    /// a client may name a group that was never created.
    [[nodiscard]] int num_client_groups() const noexcept { return next_group_id_; }

    /// Pickup-delivery pairs, in the order add_request() recorded them.
    [[nodiscard]] auto const& requests() const noexcept { return requests_; }

    /// Explicit distance entries, as an append-only log: set_distance() never
    /// overwrites, so a repeated (profile, from, to) appends and the last
    /// entry wins.  Duration and cost entries behave the same way.
    [[nodiscard]] auto const& distance_entries() const noexcept { return dist_entries_; }
    [[nodiscard]] auto const& duration_entries() const noexcept { return dur_entries_; }
    [[nodiscard]] auto const& cost_entries() const noexcept { return cost_entries_; }

    [[nodiscard]] auto const& initial_routes() const noexcept { return initial_routes_; }

    /// Pinned client ids, as pin() recorded them: no dedup, no range check.
    [[nodiscard]] auto const& pinned() const noexcept { return pinned_; }

private:
    std::vector<DepotEntry> depots_;
    std::vector<ClientEntry> clients_;
    std::vector<VehicleTypeEntry> vehicle_types_;

    // -- Pickup-delivery requests --------------------------------------------
    std::vector<std::pair<int, int>> requests_;

    // -- Client groups -------------------------------------------------------
    int next_group_id_ = 0;

    // -- Explicit matrix entries ---------------------------------------------
    std::vector<MatEntry> dist_entries_;
    std::vector<MatEntry> dur_entries_;
    std::vector<MatEntry> cost_entries_;
    int current_profile_ = 0;

    // -- Warm start ----------------------------------------------------------
    std::vector<std::vector<int>> initial_routes_;
    std::vector<int> pinned_;
};

/// Convenience: solve a CVRPLIB / VRPLIB instance file directly.
Result solve(const std::string& instance_path, TimeLimit tl);

}  // namespace coso
