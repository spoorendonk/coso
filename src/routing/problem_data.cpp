#include "routing/problem_data.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace coso {

// ---------------------------------------------------------------------------
//  Builder methods
// ---------------------------------------------------------------------------

int ProblemData::Builder::add_depot(Coord coord, DepotParams p) {
    int idx = static_cast<int>(depots_.size());
    depots_.push_back({.coord = coord, .tw = p.tw});
    return idx;
}

int ProblemData::Builder::add_client(Coord coord, ClientParams p) {
    int idx = static_cast<int>(clients_.size());
    clients_.push_back({
        .coord = coord,
        .demand = std::move(p.demand),
        .pickup = std::move(p.pickup),
        .tw = p.tw,
        .extra_tw = std::move(p.extra_tw),
        .service = p.service,
        .release_time = p.release_time,
        .prize = p.prize,
        .required = p.required,
        .group = p.group,
        .skills = std::move(p.skills),
        .client_type = p.client_type,
    });
    return idx;
}

int ProblemData::Builder::add_vehicle_type(int count, VehicleTypeParams p) {
    int idx = static_cast<int>(vehicle_types_.size());
    ensure_profile_(p.profile);
    vehicle_types_.push_back({
        .count = count,
        .capacity = std::move(p.capacity),
        .max_duration = p.max_duration,
        .max_distance = p.max_distance,
        .min_tasks = p.min_tasks,
        .max_tasks = p.max_tasks,
        .max_overtime = p.max_overtime,
        .unit_overtime_cost = p.unit_overtime_cost,
        .reload_depot = p.reload_depot,
        .max_reloads = p.max_reloads,
        .cost = p.cost,
        .profile = p.profile,
        .skills = std::move(p.skills),
    });
    return idx;
}

void ProblemData::Builder::add_request(int pickup, int delivery) {
    requests_.push_back({pickup, delivery});
}

void ProblemData::Builder::set_distance(int profile, int from, int to, int dist) {
    ensure_profile_(profile);
    dist_entries_.push_back({profile, from, to, dist});
}

void ProblemData::Builder::set_duration(int profile, int from, int to, int dur) {
    ensure_profile_(profile);
    dur_entries_.push_back({profile, from, to, dur});
}

void ProblemData::Builder::set_cost(int profile, int from, int to, int cost) {
    ensure_profile_(profile);
    cost_entries_.push_back({profile, from, to, cost});
}

// ---------------------------------------------------------------------------
//  Euclidean distance helper (rounded to nearest int, as is standard in VRP)
// ---------------------------------------------------------------------------

namespace {

int euclidean(Coord a, Coord b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return static_cast<int>(std::round(std::sqrt(dx * dx + dy * dy)));
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
//  Builder::build
// ---------------------------------------------------------------------------

ProblemData ProblemData::Builder::build(int granular_k) const {
    ProblemData pd;

    // Copy entity data.
    pd.num_depots_ = static_cast<int>(depots_.size());
    pd.num_clients_ = static_cast<int>(clients_.size());
    pd.num_vehicle_types_ = static_cast<int>(vehicle_types_.size());
    pd.num_profiles_ = max_profile_ + 1;

    pd.depots_ = depots_;
    pd.clients_ = clients_;
    pd.vehicle_types_ = vehicle_types_;
    pd.requests_ = requests_;

    // Determine number of load dimensions (max across clients and vehicle types).
    int max_dim = 0;
    for (auto const& c : pd.clients_) {
        max_dim = std::max(max_dim, static_cast<int>(c.demand.size()));
    }
    for (auto const& vt : pd.vehicle_types_) {
        max_dim = std::max(max_dim, static_cast<int>(vt.capacity.size()));
    }
    pd.num_load_dims_ = max_dim;

    // Pad demand/pickup/capacity vectors to num_load_dims_ for uniform access.
    for (auto& c : pd.clients_) {
        c.demand.resize(max_dim, 0);
        c.pickup.resize(max_dim, 0);
    }
    for (auto& vt : pd.vehicle_types_) {
        vt.capacity.resize(max_dim, 0);
    }

    int n = pd.num_nodes();

    // -------------------------------------------------------------------
    //  Build distance matrices (Euclidean default, overridden by explicit)
    // -------------------------------------------------------------------

    int mat_size = pd.num_profiles_ * n * n;
    pd.dist_matrices_.assign(mat_size, 0);
    pd.dur_matrices_.assign(mat_size, 0);
    pd.cost_matrices_.assign(mat_size, 0);

    // Compute Euclidean distances for all profiles as default.
    for (int p = 0; p < pd.num_profiles_; ++p) {
        for (int i = 0; i < n; ++i) {
            Coord ci = pd.node_coord(i);
            for (int j = 0; j < n; ++j) {
                Coord cj = pd.node_coord(j);
                int d = euclidean(ci, cj);
                int idx = p * n * n + i * n + j;
                pd.dist_matrices_[idx] = d;
                pd.dur_matrices_[idx] = d;   // duration = distance by default
                pd.cost_matrices_[idx] = d;  // cost = distance by default
            }
        }
    }

    // Override with explicit entries.
    for (auto const& e : dist_entries_) {
        int idx = e.profile * n * n + e.from * n + e.to;
        pd.dist_matrices_[idx] = e.value;
        // Also update cost if no explicit cost was set (done below).
    }
    for (auto const& e : dur_entries_) {
        int idx = e.profile * n * n + e.from * n + e.to;
        pd.dur_matrices_[idx] = e.value;
    }

    // If explicit distances were provided but no explicit cost matrix,
    // cost defaults to distance.  Apply distance entries to cost first,
    // then override with any explicit cost entries.
    for (auto const& e : dist_entries_) {
        int idx = e.profile * n * n + e.from * n + e.to;
        pd.cost_matrices_[idx] = e.value;
    }
    for (auto const& e : cost_entries_) {
        int idx = e.profile * n * n + e.from * n + e.to;
        pd.cost_matrices_[idx] = e.value;
    }

    // -------------------------------------------------------------------
    //  Granular neighbour lists (k-nearest clients for each client)
    // -------------------------------------------------------------------

    int k = std::min(granular_k, std::max(0, pd.num_clients_ - 1));
    pd.granular_k_ = k;

    if (k > 0 && pd.num_clients_ > 0) {
        pd.neighbours_.resize(pd.num_clients_ * k);

        // Reusable buffer for sorting candidates (avoids per-client allocation).
        std::vector<int> sorted_nodes;
        sorted_nodes.reserve(n - 1);

        for (int c = 0; c < pd.num_clients_; ++c) {
            int c_node = pd.num_depots_ + c;

            // Build candidate list (all nodes except self).
            sorted_nodes.clear();
            for (int i = 0; i < n; ++i) {
                if (i != c_node) {
                    sorted_nodes.push_back(i);
                }
            }

            // Sort candidates by distance from c_node (profile 0).
            auto cmp = [&](int a, int b) { return pd.dist(0, c_node, a) < pd.dist(0, c_node, b); };

            if (static_cast<int>(sorted_nodes.size()) > k) {
                std::partial_sort(sorted_nodes.begin(), sorted_nodes.begin() + k,
                                  sorted_nodes.end(), cmp);
            } else {
                std::sort(sorted_nodes.begin(), sorted_nodes.end(), cmp);
            }

            int count = std::min(k, static_cast<int>(sorted_nodes.size()));
            for (int i = 0; i < count; ++i) {
                pd.neighbours_[c * k + i] = sorted_nodes[i];
            }
            // Fill remaining with -1 if fewer candidates than k.
            for (int i = count; i < k; ++i) {
                pd.neighbours_[c * k + i] = -1;
            }
        }
    }

    return pd;
}

}  // namespace coso
