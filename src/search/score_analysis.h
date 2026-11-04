#pragma once

#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"

#include <cstdint>
#include <string>
#include <vector>

namespace coso {

/// Per-route cost breakdown for debugging and analysis.
struct RouteAnalysis {
    int route_idx;
    int vehicle_type;
    std::vector<int> clients;
    Cost distance;
    Cost load_excess;
    Cost objective;
    Cost penalty;
    std::vector<int> total_demand;  // per dimension
    std::vector<int> capacity;      // per dimension
};

/// Full solution cost breakdown.
struct SolutionAnalysis {
    std::vector<RouteAnalysis> routes;
    Cost total_objective;
    Cost total_penalty;
    Cost penalized_cost;
    int num_routes_used;
    int num_unserved;
    bool feasible;

    /// Human-readable formatted summary.
    [[nodiscard]] std::string to_string() const;
};

/// Analyze a solution and produce a detailed cost breakdown.
[[nodiscard]] SolutionAnalysis analyze(Solution const& sol, CostEvaluator const& eval,
                                       ProblemData const& data);

}  // namespace coso
