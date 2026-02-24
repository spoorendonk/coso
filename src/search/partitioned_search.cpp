#include "search/partitioned_search.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <numeric>
#include <set>

namespace coso {

// ---------------------------------------------------------------------------
//  Construction
// ---------------------------------------------------------------------------

PartitionedSearch::PartitionedSearch(ProblemData const& data, unsigned int seed)
    : data_(&data), rng_(seed)
{
}

// ---------------------------------------------------------------------------
//  K-means partitioning
// ---------------------------------------------------------------------------

Partition PartitionedSearch::partition_clients(int k, int kmeans_iters)
{
    int const n = data_->num_clients();
    assert(k > 0);
    assert(n > 0);

    // Clamp k to number of clients.
    k = std::min(k, n);

    Partition result;
    result.assignment.resize(n, 0);
    result.clusters.resize(k);

    if (k == 1) {
        // Trivial: all clients in one cluster.
        result.clusters[0].resize(n);
        std::iota(result.clusters[0].begin(), result.clusters[0].end(), 0);
        return result;
    }

    // Initialize centroids using k-means++ style: pick k random distinct clients.
    std::vector<double> cx(k), cy(k);
    {
        std::vector<int> indices(n);
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), rng_);
        for (int i = 0; i < k; ++i) {
            auto coord = data_->client(indices[i]).coord;
            cx[i] = coord.x;
            cy[i] = coord.y;
        }
    }

    // K-means iterations.
    for (int iter = 0; iter < kmeans_iters; ++iter) {
        // Assignment step: assign each client to nearest centroid.
        for (int c = 0; c < n; ++c) {
            auto coord = data_->client(c).coord;
            double best_dist = std::numeric_limits<double>::max();
            int best_k = 0;
            for (int j = 0; j < k; ++j) {
                double dx = coord.x - cx[j];
                double dy = coord.y - cy[j];
                double d = dx * dx + dy * dy;
                if (d < best_dist) {
                    best_dist = d;
                    best_k = j;
                }
            }
            result.assignment[c] = best_k;
        }

        // Update step: recompute centroids.
        std::vector<double> sum_x(k, 0.0), sum_y(k, 0.0);
        std::vector<int> count(k, 0);
        for (int c = 0; c < n; ++c) {
            int cluster = result.assignment[c];
            auto coord = data_->client(c).coord;
            sum_x[cluster] += coord.x;
            sum_y[cluster] += coord.y;
            count[cluster]++;
        }
        for (int j = 0; j < k; ++j) {
            if (count[j] > 0) {
                cx[j] = sum_x[j] / count[j];
                cy[j] = sum_y[j] / count[j];
            }
            // If a cluster is empty, keep its old centroid (it may attract
            // clients in a later iteration).
        }
    }

    // Build cluster lists from final assignment.
    for (auto& cl : result.clusters) cl.clear();
    for (int c = 0; c < n; ++c) {
        result.clusters[result.assignment[c]].push_back(c);
    }

    return result;
}

// ---------------------------------------------------------------------------
//  Overlap expansion
// ---------------------------------------------------------------------------

std::vector<std::vector<int>> PartitionedSearch::expand_with_overlap(
    Partition const& part, double overlap_frac) const
{
    int const k = part.num_clusters();
    int const n = data_->num_clients();

    // Start with a copy of the base clusters.
    std::vector<std::vector<int>> expanded = part.clusters;

    if (overlap_frac <= 0.0 || k <= 1) {
        return expanded;
    }

    // Compute squared distance from each client to each centroid.
    // Centroids are the mean of each cluster's coordinates.
    std::vector<double> cx(k, 0.0), cy(k, 0.0);
    for (int j = 0; j < k; ++j) {
        for (int c : part.clusters[j]) {
            auto coord = data_->client(c).coord;
            cx[j] += coord.x;
            cy[j] += coord.y;
        }
        if (!part.clusters[j].empty()) {
            auto sz = static_cast<double>(part.clusters[j].size());
            cx[j] /= sz;
            cy[j] /= sz;
        }
    }

    // For each cluster, find boundary clients from other clusters to add.
    for (int j = 0; j < k; ++j) {
        int max_overlap = static_cast<int>(
            std::ceil(overlap_frac * static_cast<double>(part.clusters[j].size())));
        if (max_overlap <= 0) continue;

        // Collect clients NOT in this cluster, with their distance to this centroid.
        std::vector<std::pair<double, int>> candidates;
        candidates.reserve(n - static_cast<int>(part.clusters[j].size()));
        for (int c = 0; c < n; ++c) {
            if (part.assignment[c] == j) continue;
            auto coord = data_->client(c).coord;
            double dx = coord.x - cx[j];
            double dy = coord.y - cy[j];
            candidates.emplace_back(dx * dx + dy * dy, c);
        }

        // Take the closest max_overlap candidates.
        if (static_cast<int>(candidates.size()) > max_overlap) {
            std::partial_sort(
                candidates.begin(),
                candidates.begin() + max_overlap,
                candidates.end());
            candidates.resize(max_overlap);
        }

        for (auto const& [dist, c] : candidates) {
            expanded[j].push_back(c);
        }
    }

    return expanded;
}

// ---------------------------------------------------------------------------
//  Sub-solution extraction
// ---------------------------------------------------------------------------

Solution PartitionedSearch::extract_sub_solution_(
    Solution const& global,
    std::vector<int> const& clients) const
{
    // Create a set for quick lookup.
    std::set<int> client_set(clients.begin(), clients.end());

    Solution sub(*data_);

    // For each route in the global solution, extract only the clients in our set.
    for (int r = 0; r < global.num_routes(); ++r) {
        auto const& route = global.route(r);
        std::vector<int> route_clients;
        for (int c : route.clients()) {
            if (client_set.contains(c)) {
                route_clients.push_back(c);
            }
        }
        if (!route_clients.empty()) {
            sub.set_route_clients(r, std::move(route_clients));
        }
    }

    return sub;
}

// ---------------------------------------------------------------------------
//  Sub-solution merge
// ---------------------------------------------------------------------------

Solution PartitionedSearch::merge_sub_solutions_(
    std::vector<Solution> const& subs,
    std::vector<std::vector<int>> const& expanded_clusters,
    CostEvaluator const& eval) const
{
    int const n = data_->num_clients();
    int const num_routes = data_->total_vehicles();

    // For each client, find which sub-solution and route gives the best cost.
    // Track: client -> (sub_idx, route_idx, position_in_route).
    struct ClientPlacement {
        int sub_idx   = -1;
        int route_idx = -1;
        int pos       = -1;
    };
    std::vector<ClientPlacement> best_placement(n);

    // For each sub-solution, record where each client ended up.
    for (int s = 0; s < static_cast<int>(subs.size()); ++s) {
        auto const& sub = subs[s];
        for (int r = 0; r < sub.num_routes(); ++r) {
            auto const& route = sub.route(r);
            for (int p = 0; p < route.size(); ++p) {
                int c = route.client(p);
                auto& bp = best_placement[c];
                if (bp.sub_idx < 0) {
                    // First time seeing this client.
                    bp = {s, r, p};
                } else {
                    // Client appears in multiple sub-solutions (overlap).
                    // Pick the one with lower route cost.
                    auto const& old_route = subs[bp.sub_idx].route(bp.route_idx);
                    auto const& new_route = route;
                    if (eval.route_cost(new_route) < eval.route_cost(old_route)) {
                        bp = {s, r, p};
                    }
                }
            }
        }
    }

    // Build the merged solution by reconstructing routes.
    // Group clients by their chosen (sub_idx, route_idx), preserving order.
    // Use a map from route_idx -> ordered list of clients (from the winning sub).
    std::vector<std::vector<int>> route_clients(num_routes);

    for (int c = 0; c < n; ++c) {
        auto const& bp = best_placement[c];
        if (bp.sub_idx >= 0) {
            route_clients[bp.route_idx].push_back(c);
        }
    }

    // Ensure clients are in the same order as in the winning sub-solution's route.
    for (int r = 0; r < num_routes; ++r) {
        if (route_clients[r].empty()) continue;

        // Collect all sub-solutions that contribute to this route.
        // For each client in this route, it came from subs[bp.sub_idx].route(r).
        // We need to order them according to their position in their sub-solution.
        auto& rc = route_clients[r];
        std::sort(rc.begin(), rc.end(), [&](int a, int b) {
            auto const& pa = best_placement[a];
            auto const& pb = best_placement[b];
            // If from same sub, use position order.
            if (pa.sub_idx == pb.sub_idx) return pa.pos < pb.pos;
            // Otherwise, order by sub index (arbitrary but consistent).
            return pa.sub_idx < pb.sub_idx;
        });
    }

    Solution merged(*data_);
    for (int r = 0; r < num_routes; ++r) {
        if (!route_clients[r].empty()) {
            merged.set_route_clients(r, std::move(route_clients[r]));
        }
    }

    // Handle any unassigned clients (shouldn't happen if all subs cover all
    // clients, but handle gracefully).  Insert them greedily.
    if (merged.num_unassigned() > 0) {
        auto unassigned = std::vector<int>(
            merged.unassigned().begin(), merged.unassigned().end());
        for (int c : unassigned) {
            // Find cheapest insertion across all routes.
            int64_t best_delta = std::numeric_limits<int64_t>::max();
            int best_route = -1;
            int best_pos = -1;
            for (int r = 0; r < num_routes; ++r) {
                auto const& route = merged.route(r);
                for (int p = 0; p <= route.size(); ++p) {
                    int64_t delta = eval.eval_insert_cost(route, p, c);
                    if (delta < best_delta) {
                        best_delta = delta;
                        best_route = r;
                        best_pos = p;
                    }
                }
            }
            if (best_route >= 0) {
                merged.insert_client(best_route, best_pos, c);
            }
        }
    }

    return merged;
}

// ---------------------------------------------------------------------------
//  Main run loop
// ---------------------------------------------------------------------------

Solution PartitionedSearch::run(
    Solution const& initial,
    CostEvaluator const& eval,
    LocalSearchFn const& local_search,
    PartitionConfig const& config)
{
    Solution best = initial;
    int64_t best_cost = best.cost(eval);

    for (int iter = 0; iter < config.max_iterations; ++iter) {
        // 1. Partition clients.
        auto part = partition_clients(config.num_partitions,
                                      config.kmeans_iters);

        // 2. Expand with overlap.
        auto expanded = expand_with_overlap(part, config.overlap_frac);

        // 3. Solve each partition independently.
        std::vector<Solution> sub_solutions;
        sub_solutions.reserve(config.num_partitions);

        for (int k = 0; k < part.num_clusters(); ++k) {
            if (expanded[k].empty()) {
                // Empty cluster -- push an empty sub-solution.
                sub_solutions.emplace_back(*data_);
                continue;
            }
            auto sub = extract_sub_solution_(best, expanded[k]);
            local_search(sub, eval);
            sub_solutions.push_back(std::move(sub));
        }

        // 4. Merge sub-solutions.
        auto merged = merge_sub_solutions_(sub_solutions, expanded, eval);

        // 5. Apply local search to the merged solution (global improvement).
        local_search(merged, eval);

        // 6. Update best.
        int64_t merged_cost = merged.cost(eval);
        if (merged_cost < best_cost) {
            best = std::move(merged);
            best_cost = merged_cost;
        }
    }

    return best;
}

} // namespace coso
