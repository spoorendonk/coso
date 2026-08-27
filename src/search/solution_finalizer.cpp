#include "search/solution_finalizer.h"

#include "routing/cost_evaluator.h"
#include "search/stop_criterion.h"

#include <limits>

namespace coso {

SolutionFinalizer::SolutionFinalizer(ProblemData const& data) : data_(&data), ls_(data) {}

void SolutionFinalizer::finalize(Solution& sol, StopCriterion* stop) {
    // Very high penalty weights make any constraint violation prohibitively
    // expensive, so local search will only accept feasible moves.
    constexpr int kHighPenalty = 1'000'000;
    CostEvaluator eval(kHighPenalty, kHighPenalty, kHighPenalty);

    // Phase 1: run local search with high penalties.  If the solution is
    // already feasible this may still improve distance via inter-route moves
    // that were not worthwhile under the lower penalties used during search.
    ls_.run(sol, eval, stop);

    // Phase 2: if the solution is still infeasible, repair by removing
    // excess clients from overloaded routes.
    if (!sol.feasible() && !(stop && stop->should_stop())) {
        repair_infeasible_(sol);

        // Phase 3: run local search again — the removed clients freed
        // capacity, so new improving moves may exist.
        ls_.run(sol, eval, stop);
    }
}

void SolutionFinalizer::repair_infeasible_(Solution& sol) {
    CostEvaluator eval_zero(0, 0, 0);

    for (int v = 0; v < sol.num_routes(); ++v) {
        Route& route = sol.route(v);

        // Repeatedly remove the client whose removal most reduces load
        // excess until the route is feasible.
        while (route.load_excess() > 0) {
            // Find the client whose removal most reduces the cost.  With
            // zero-penalty eval, cost is pure distance, so we pick the
            // client whose removal gives the best (most negative) distance
            // delta.  Among those, prefer one that actually reduces load
            // excess the most.
            int best_pos = -1;
            int best_excess_after = std::numeric_limits<int>::max();
            int best_dist_delta = std::numeric_limits<int>::max();

            for (int p = 0; p < route.size(); ++p) {
                int excess_after = route.eval_remove_load(p);
                int dist_delta = route.eval_remove_distance(p);

                // Prefer removing clients that reduce excess the most.
                // Break ties by distance delta (prefer larger distance
                // savings).
                bool better = (excess_after < best_excess_after) ||
                              (excess_after == best_excess_after && dist_delta < best_dist_delta);

                if (better) {
                    best_pos = p;
                    best_excess_after = excess_after;
                    best_dist_delta = dist_delta;
                }
            }

            if (best_pos < 0) {
                break;  // Shouldn't happen, but guard against infinite loop.
            }

            sol.remove_client(v, best_pos);
        }
    }
}

}  // namespace coso
