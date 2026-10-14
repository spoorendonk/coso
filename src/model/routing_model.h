#pragma once

#include "types.h"

#include <climits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace coso {

/// Parameters for a vehicle type.
struct VehicleTypeParams {
    std::vector<int> capacity;  ///< N load dimensions
    /// 0 = unlimited.  Accepted and dropped: never penalised, and a route over
    /// it comes back feasible (#194).
    int max_duration = 0;
    /// 0 = unlimited.  Accepted and dropped, as max_duration (#194).
    int max_distance = 0;
};

/// Parameters for a client location.
struct ClientParams {
    std::vector<int> demand;  ///< N load dimensions (matches capacity)
    std::vector<int> pickup;  ///< backhaul pickup quantities
    TimeWindow tw = {0, INT_MAX};
    int service = 0;  ///< service duration
    int prize = 0;    ///< summed into Result::prize when served; see RoutingObjective
    /// Accepted and dropped: the native engine's served set ignores it both
    /// ways.  Construction tries to serve every client, false or not, and the
    /// final capacity repair strips clients from an overloaded route without
    /// trying other routes, so a client with true can be left in
    /// Result::unserved at no cost, with the solution still reported feasible
    /// (#196).
    bool required = true;
};

/// Parameters for a depot location.
struct DepotParams {
    TimeWindow tw = {0, INT_MAX};
};

/// What a routing model optimises.  Result::cost() is lower-is-better under
/// each; the comment on each value says which Result fields score it.
enum class RoutingObjective {
    /// Minimise distance minus the prizes of served clients (CVRPLIB,
    /// Cordeau, Li & Lim).  Scored by Result::cost().
    MinDistance,
    /// Minimise the number of routes, then distance (Solomon, Gehring &
    /// Homberger).  Scored by the pair (Result::num_routes(),
    /// Result::distance()), compared lexicographically; Result::cost() is the
    /// distance.
    MinVehiclesThenDistance,
    /// Maximise the prize collected from served clients, subject to each
    /// vehicle's max_distance or max_duration as a budget (OPLib, Chao TOP);
    /// both budgets are themselves accepted and dropped by the native engine,
    /// so a returned route can break one and still read feasible (#194).
    /// Clients that may be skipped declare required = false.  Scored by
    /// Result::prize(); Result::cost() is its negation.
    MaxPrize,
};

/// Routing model: declare depots, vehicles, clients, distances, then solve.
///
/// What the native engine enforces is CVRP: demand against an N-dimensional
/// vehicle capacity, minimising distance minus the prizes of served clients,
/// which is what Result::cost reports under the default MinDistance; Result::distance is the bare
/// distance. The schema is the 13 slots the v1 scope ruling on #200 kept on benchmark evidence.
/// Penalised but not enforced by the native engine: client and depot time windows, which service
/// times feed (#194).  Accepted and dropped: max_distance and max_duration (#194),
/// ClientParams::required, add_request and every depot after the first (#196), and any objective
/// but MinDistance (#238) -- the search minimises distance minus prizes whatever is declared, while
/// Result is still computed under the declared objective.
class RoutingModel {
public:
    // -- Stored entry types --------------------------------------------------
    //
    //  What the model keeps for each declaration, returned by the accessors
    //  below so a backend adapter can read a model back.

    /// A depot as declared.
    struct DepotEntry {
        double x = 0.0, y = 0.0;
        DepotParams params;
    };

    /// A client as declared.  add_pickup / add_delivery are aliases for
    /// add_client, so the pickup/delivery role is not stored here — only the
    /// pairing recorded by add_request, see requests().
    struct ClientEntry {
        double x = 0.0, y = 0.0;
        ClientParams params;
    };

    /// A vehicle type as declared, with its fleet count.
    struct VehicleTypeEntry {
        int count = 0;
        VehicleTypeParams params;
    };

    /// One explicit distance / duration matrix entry.
    struct MatEntry {
        int from;
        int to;
        int value;
    };

    /// Add a depot at the given coordinates.  Only the first depot is used:
    /// every route starts and ends at depot 0, so a second depot is accepted
    /// and dropped (#196).
    int add_depot(double x, double y, DepotParams p = {});

    /// Add a vehicle type with the given count and parameters.
    int add_vehicle_type(int count, VehicleTypeParams p = {});

    /// Add a client at the given coordinates.
    int add_client(double x, double y, ClientParams p = {});

    /// Add a pickup location (for pickup-delivery problems).
    int add_pickup(double x, double y, ClientParams p = {});

    /// Add a delivery location (for pickup-delivery problems).
    int add_delivery(double x, double y, ClientParams p = {});

    /// Link a pickup-delivery pair: both on the same route, pickup before
    /// delivery.  Accepted and dropped: the native engine routes the two
    /// clients independently, so neither condition is enforced (#196).
    void add_request(int pickup, int delivery);

    /// Convenience alias for add_request.
    void add_pickup_delivery(int pickup, int delivery);

    // -- Distance / duration matrices ----------------------------------------

    /// Set the distance between two nodes.
    void set_distance(int from, int to, int dist);

    /// Set the travel duration between two nodes.
    void set_duration(int from, int to, int dur);

    // -- Objective -----------------------------------------------------------

    /// Set the objective (default: MinDistance).  The native engine honours
    /// MinDistance only; the others are accepted and dropped (#238).
    void set_objective(RoutingObjective obj) { objective_ = obj; }

    // -- Solve ---------------------------------------------------------------

    /// Solve the routing problem within the given time limit.
    Result solve(TimeLimit tl);

    // -- Accessors -----------------------------------------------------------

    [[nodiscard]] int num_depots() const noexcept { return static_cast<int>(depots_.size()); }
    [[nodiscard]] DepotEntry const& depot(int d) const {
        if (d < 0 || static_cast<size_t>(d) >= depots_.size()) {
            throw std::out_of_range("RoutingModel::depot: invalid index");
        }
        return depots_[d];
    }

    [[nodiscard]] int num_clients() const noexcept { return static_cast<int>(clients_.size()); }
    [[nodiscard]] ClientEntry const& client(int c) const {
        if (c < 0 || static_cast<size_t>(c) >= clients_.size()) {
            throw std::out_of_range("RoutingModel::client: invalid index");
        }
        return clients_[c];
    }

    [[nodiscard]] int num_vehicle_types() const noexcept {
        return static_cast<int>(vehicle_types_.size());
    }
    [[nodiscard]] VehicleTypeEntry const& vehicle_type(int v) const {
        if (v < 0 || static_cast<size_t>(v) >= vehicle_types_.size()) {
            throw std::out_of_range("RoutingModel::vehicle_type: invalid index");
        }
        return vehicle_types_[v];
    }

    [[nodiscard]] RoutingObjective objective() const noexcept { return objective_; }

    /// Pickup-delivery pairs, in the order add_request() recorded them.
    [[nodiscard]] auto const& requests() const noexcept { return requests_; }

    /// Explicit distance entries, as an append-only log: set_distance() never
    /// overwrites, so a repeated (from, to) appends and the last entry wins.
    /// Duration entries behave the same way.
    [[nodiscard]] auto const& distance_entries() const noexcept { return dist_entries_; }
    [[nodiscard]] auto const& duration_entries() const noexcept { return dur_entries_; }

private:
    std::vector<DepotEntry> depots_;
    std::vector<ClientEntry> clients_;
    std::vector<VehicleTypeEntry> vehicle_types_;

    // -- Pickup-delivery requests --------------------------------------------
    std::vector<std::pair<int, int>> requests_;

    // -- Explicit matrix entries ---------------------------------------------
    std::vector<MatEntry> dist_entries_;
    std::vector<MatEntry> dur_entries_;

    RoutingObjective objective_ = RoutingObjective::MinDistance;
};

/// Convenience: solve a CVRPLIB / VRPLIB instance file directly.
Result solve(const std::string& instance_path, TimeLimit tl);

}  // namespace coso
