#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <cstdint>
#include <functional>
#include <random>
#include <vector>

namespace coso {

/// Configuration for partitioned search.
struct PartitionConfig {
    int num_partitions = 4;     ///< Number of partitions (clusters).
    double overlap_frac = 0.1;  ///< Fraction of boundary clients shared between
                                ///< adjacent partitions (0.0 = no overlap).
    int max_iterations = 10;    ///< Maximum partitioned-search iterations.
    int kmeans_iters = 20;      ///< K-means iterations for clustering.
};

/// Result of partitioning clients into clusters.
struct Partition {
    /// partition[i] = cluster index (0..k-1) for client i.
    std::vector<int> assignment;

    /// clusters[k] = list of client indices in cluster k.
    std::vector<std::vector<int>> clusters;

    /// Number of clusters.
    [[nodiscard]] int num_clusters() const noexcept { return static_cast<int>(clusters.size()); }
};

/// Partitioned (decomposition-based) search for large routing instances.
///
/// Decomposes the client set into geographic clusters using k-means on
/// coordinates, then solves each sub-problem independently using a provided
/// local search callable, and merges the sub-solutions back into a global
/// solution.
///
/// Overlap: boundary clients (those closest to a neighbouring cluster
/// centroid) are included in both partitions, allowing the local search
/// to improve boundary assignments.  During merge, each overlapping client
/// is assigned to the route where it has the lowest insertion cost.
///
/// Algorithm:
///   1. Partition clients into k clusters via k-means.
///   2. Expand each cluster with overlap (boundary clients).
///   3. For each cluster, extract a sub-solution and apply the local
///      search callable.
///   4. Merge sub-solutions: for each route in each sub-solution, insert
///      its clients into the global solution.
///   5. Repeat for max_iterations (re-partitioning each time with a slight
///      random perturbation to explore different decompositions).
///   6. Return the best global solution found.
class PartitionedSearch {
public:
    /// Callable type for the local search procedure applied to each partition.
    /// Takes a mutable Solution& and CostEvaluator, improves in place.
    using LocalSearchFn = std::function<void(Solution&, CostEvaluator const&)>;

    /// Construct a partitioned search for the given problem.
    ///
    /// @param data   The compiled problem data.
    /// @param seed   Random seed for k-means initialization.
    explicit PartitionedSearch(ProblemData const& data, unsigned int seed = 42);

    /// Run the partitioned search.
    ///
    /// @param initial   Starting solution (all clients should be assigned).
    /// @param eval      Cost evaluator.
    /// @param local_search  Callable that improves a sub-solution in place.
    /// @param config    Partition configuration.
    /// @return The best solution found.
    [[nodiscard]] Solution run(Solution const& initial, CostEvaluator const& eval,
                               LocalSearchFn const& local_search,
                               PartitionConfig const& config = {});

    /// Partition clients into k clusters using k-means on coordinates.
    ///
    /// @param k            Number of clusters.
    /// @param kmeans_iters Number of k-means iterations.
    /// @return Partition assignment and cluster lists.
    [[nodiscard]] Partition partition_clients(int k, int kmeans_iters = 20);

    /// Expand clusters with overlapping boundary clients.
    ///
    /// For each cluster, adds the closest clients from neighbouring
    /// clusters (up to overlap_frac * cluster_size additional clients).
    ///
    /// @param part          The base partition.
    /// @param overlap_frac  Fraction of overlap (0.0 to 1.0).
    /// @return Expanded clusters (may have duplicates across clusters).
    [[nodiscard]] std::vector<std::vector<int>> expand_with_overlap(Partition const& part,
                                                                    double overlap_frac) const;

private:
    ProblemData const* data_;
    std::mt19937 rng_;

    /// Extract a sub-solution: only clients in the given set are assigned;
    /// all others are removed from routes.
    [[nodiscard]] Solution extract_sub_solution_(Solution const& global,
                                                 std::vector<int> const& clients) const;

    /// Merge sub-solutions back into a single global solution.
    /// Each client is placed in the route (from any sub-solution) where
    /// it appeared; conflicts (overlap clients) resolved by lowest cost.
    [[nodiscard]] Solution merge_sub_solutions_(
        std::vector<Solution> const& subs, std::vector<std::vector<int>> const& expanded_clusters,
        CostEvaluator const& eval) const;
};

}  // namespace coso
