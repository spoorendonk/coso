#pragma once

#include "model/routing_model.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <span>
#include <vector>

namespace coso {

/// Compiled, immutable representation of a routing instance.
///
/// Built once from a RoutingModel (or directly via ProblemData::Builder).
/// Provides efficient, cache-friendly access patterns for local search:
///   - Contiguous distance/duration matrices (row-major, per profile)
///   - Contiguous client/depot/vehicle attribute arrays
///   - Precomputed granular neighbour lists (k-nearest per client)
///
/// Node numbering: depots are indexed 0..num_depots-1, clients are indexed
/// num_depots..num_depots+num_clients-1.  The distance matrix is sized
/// (num_depots + num_clients) x (num_depots + num_clients).
class ProblemData {
public:
    // -------------------------------------------------------------------
    //  Client data (contiguous struct-of-arrays for cache efficiency)
    // -------------------------------------------------------------------

    struct ClientData {
        Coord coord            = {0.0, 0.0};
        std::vector<int> demand;          ///< N load dimensions
        std::vector<int> pickup;          ///< backhaul pickup quantities
        TimeWindow tw          = {0, INT_MAX};
        std::vector<TimeWindow> extra_tw; ///< additional time windows
        int service            = 0;
        int release_time       = 0;
        int prize              = 0;
        bool required          = true;
        int group              = -1;
        int quantity           = 0;
        std::vector<std::string> skills;
        int setup_time         = 0;
        int location           = -1;
    };

    struct DepotData {
        Coord coord     = {0.0, 0.0};
        TimeWindow tw   = {0, INT_MAX};
    };

    struct VehicleTypeData {
        int count                = 0;
        std::vector<int> capacity;
        int max_duration         = 0;       ///< 0 = unlimited
        int max_distance         = 0;       ///< 0 = unlimited
        int max_tasks            = 0;       ///< 0 = unlimited
        int max_overtime         = 0;
        int unit_overtime_cost   = 0;
        int reload_depot         = -1;
        int max_reloads          = 0;
        CostParams cost;
        int profile              = 0;
        double speed_factor      = 1.0;
        std::vector<std::string> skills;
    };

    /// A pickup-delivery request: both on same route, pickup before delivery.
    struct Request {
        int pickup;
        int delivery;
    };

    // -------------------------------------------------------------------
    //  Builder — the only way to construct a ProblemData
    // -------------------------------------------------------------------

    class Builder {
    public:
        /// Add a depot. Returns depot index (0-based).
        int add_depot(Coord coord, DepotParams p = {});

        /// Add a client. Returns client index (0-based, offset from depots
        /// in the final node numbering).
        int add_client(Coord coord, ClientParams p = {});

        /// Add a vehicle type. Returns vehicle type index.
        int add_vehicle_type(int count, VehicleTypeParams p = {});

        /// Add a pickup-delivery request.
        void add_request(int pickup, int delivery);

        /// Set explicit distance for profile/from/to.  If not called,
        /// Euclidean distances are computed from coordinates.
        void set_distance(int profile, int from, int to, int dist);

        /// Set explicit duration for profile/from/to.
        void set_duration(int profile, int from, int to, int dur);

        /// Set explicit cost-matrix entry for profile/from/to.
        void set_cost(int profile, int from, int to, int cost);

        /// Build the immutable ProblemData.
        /// @param granular_k  Number of nearest neighbours per client
        ///                    for granular neighbourhood (0 = skip).
        [[nodiscard]] ProblemData build(int granular_k = 40) const;

    private:
        std::vector<DepotData>       depots_;
        std::vector<ClientData>      clients_;
        std::vector<VehicleTypeData> vehicle_types_;
        std::vector<Request>         requests_;

        // Explicit matrices: profile -> flat row-major matrix.
        // Lazily sized when set_distance/set_duration/set_cost is called.
        struct MatrixEntry { int profile; int from; int to; int value; };
        std::vector<MatrixEntry> dist_entries_;
        std::vector<MatrixEntry> dur_entries_;
        std::vector<MatrixEntry> cost_entries_;

        /// Maximum profile index seen.
        int max_profile_ = 0;

        void ensure_profile_(int profile) {
            if (profile > max_profile_) max_profile_ = profile;
        }
    };

    // -------------------------------------------------------------------
    //  Accessors (all const — ProblemData is immutable after construction)
    // -------------------------------------------------------------------

    [[nodiscard]] int num_depots()        const noexcept { return num_depots_; }
    [[nodiscard]] int num_clients()       const noexcept { return num_clients_; }
    [[nodiscard]] int num_vehicle_types() const noexcept { return num_vehicle_types_; }
    [[nodiscard]] int num_nodes()         const noexcept { return num_depots_ + num_clients_; }
    [[nodiscard]] int num_profiles()      const noexcept { return num_profiles_; }
    [[nodiscard]] int num_load_dims()     const noexcept { return num_load_dims_; }

    /// Client data for client index c (0-based among clients).
    [[nodiscard]] ClientData const& client(int c) const {
        assert(c >= 0 && c < num_clients_);
        return clients_[c];
    }

    /// Depot data for depot index d (0-based).
    [[nodiscard]] DepotData const& depot(int d) const {
        assert(d >= 0 && d < num_depots_);
        return depots_[d];
    }

    /// Vehicle type data for vehicle type index v.
    [[nodiscard]] VehicleTypeData const& vehicle_type(int v) const {
        assert(v >= 0 && v < num_vehicle_types_);
        return vehicle_types_[v];
    }

    /// Pickup-delivery requests.
    [[nodiscard]] std::span<Request const> requests() const noexcept {
        return requests_;
    }

    /// Distance from node i to node j under the given profile.
    /// Node numbering: depots 0..num_depots-1, clients num_depots..num_nodes-1.
    [[nodiscard]] int dist(int profile, int from, int to) const {
        assert(profile >= 0 && profile < num_profiles_);
        int n = num_nodes();
        assert(from >= 0 && from < n && to >= 0 && to < n);
        return dist_matrices_[profile * n * n + from * n + to];
    }

    /// Distance for the default profile (0).
    [[nodiscard]] int dist(int from, int to) const {
        return dist(0, from, to);
    }

    /// Duration from node i to node j under the given profile.
    [[nodiscard]] int dur(int profile, int from, int to) const {
        assert(profile >= 0 && profile < num_profiles_);
        int n = num_nodes();
        assert(from >= 0 && from < n && to >= 0 && to < n);
        return dur_matrices_[profile * n * n + from * n + to];
    }

    /// Duration for the default profile (0).
    [[nodiscard]] int dur(int from, int to) const {
        return dur(0, from, to);
    }

    /// Cost-matrix entry from node i to node j under the given profile.
    /// Falls back to distance if no explicit cost matrix was provided.
    [[nodiscard]] int cost(int profile, int from, int to) const {
        assert(profile >= 0 && profile < num_profiles_);
        int n = num_nodes();
        assert(from >= 0 && from < n && to >= 0 && to < n);
        return cost_matrices_[profile * n * n + from * n + to];
    }

    /// Cost for the default profile (0).
    [[nodiscard]] int cost(int from, int to) const {
        return cost(0, from, to);
    }

    /// Granular neighbour list for client c (0-based among clients).
    /// Returns a span of node indices (in full node numbering) sorted by
    /// increasing distance.  May be empty if granular_k was 0.
    [[nodiscard]] std::span<int const> neighbours(int c) const {
        assert(c >= 0 && c < num_clients_);
        int start = c * granular_k_;
        int end   = start + granular_k_;
        return {neighbours_.data() + start, neighbours_.data() + end};
    }

    /// The k used for granular neighbour lists.
    [[nodiscard]] int granular_k() const noexcept { return granular_k_; }

    /// Total number of vehicles across all types.
    [[nodiscard]] int total_vehicles() const noexcept {
        int total = 0;
        for (auto const& vt : vehicle_types_)
            total += vt.count;
        return total;
    }

    /// Location coordinate for a node (depot or client) in full numbering.
    [[nodiscard]] Coord node_coord(int node) const {
        assert(node >= 0 && node < num_nodes());
        if (node < num_depots_)
            return depots_[node].coord;
        return clients_[node - num_depots_].coord;
    }

private:
    int num_depots_        = 0;
    int num_clients_       = 0;
    int num_vehicle_types_ = 0;
    int num_profiles_      = 1;
    int num_load_dims_     = 0;
    int granular_k_        = 0;

    std::vector<ClientData>      clients_;
    std::vector<DepotData>       depots_;
    std::vector<VehicleTypeData> vehicle_types_;
    std::vector<Request>         requests_;

    // Flat row-major matrices: profile * n * n + from * n + to.
    std::vector<int> dist_matrices_;
    std::vector<int> dur_matrices_;
    std::vector<int> cost_matrices_;

    // Flat neighbour lists: client c's neighbours at [c * granular_k_ .. (c+1)*granular_k_).
    std::vector<int> neighbours_;

    // Construction helper — only Builder can create.
    ProblemData() = default;
    friend class Builder;
};

} // namespace coso
