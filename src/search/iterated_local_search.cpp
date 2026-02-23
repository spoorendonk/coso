#include "search/iterated_local_search.h"

#include "routing/construction.h"
#include "routing/local_search.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace coso {

IteratedLocalSearch::IteratedLocalSearch(ProblemData const& data,
                                         unsigned int seed)
    : data_(&data),
      rng_(seed)
{
}

Solution IteratedLocalSearch::run(CostEvaluator const& eval,
                                  StopCriterion& stop)
{
    // Step 1: Construct initial solution.
    Solution current = construction::clarke_wright(*data_, eval);

    // Step 2: Local search to local optimum.
    LocalSearch ls(*data_);
    ls.run(current, eval);

    // Track the best solution found.
    Solution best = current;
    int64_t best_cost = best.cost(eval);

    // Initialize Late Acceptance fitness list.
    // All entries start at the initial solution cost.
    int64_t current_cost = current.cost(eval);
    std::vector<int64_t> la_list(la_length_, current_cost);
    int la_index = 0;

    // Main ILS loop.
    while (!stop.should_stop()) {
        // Step 3: Perturb (ruin-and-recreate).
        Solution candidate = current;
        perturb_(candidate, eval);

        // Step 4: Local search.
        ls.run(candidate, eval);

        int64_t candidate_cost = candidate.cost(eval);

        // Step 5: Late Acceptance criterion.
        // Accept if candidate cost <= cost from L iterations ago.
        int64_t la_cost = la_list[la_index];
        if (candidate_cost <= la_cost || candidate_cost <= current_cost) {
            current = std::move(candidate);
            current_cost = current.cost(eval);
        }

        // Update late acceptance list.
        la_list[la_index] = current_cost;
        la_index = (la_index + 1) % la_length_;

        // Update best.
        if (current_cost < best_cost) {
            best = current;
            best_cost = current_cost;
            stop.improvement();
        }

        // Step 6: Advance iteration.
        stop.iteration();
    }

    return best;
}

// ---------------------------------------------------------------------------
//  Perturbation: ruin-and-recreate
// ---------------------------------------------------------------------------

void IteratedLocalSearch::perturb_(Solution& sol, CostEvaluator const& eval)
{
    auto removed = ruin_(sol);
    recreate_(sol, removed, eval);
}

std::vector<int> IteratedLocalSearch::ruin_(Solution& sol)
{
    int num_clients = data_->num_clients();
    if (num_clients == 0)
        return {};

    // Determine k: random in [ruin_frac_min, ruin_frac_max] * num_clients.
    int k_min = std::max(1, static_cast<int>(ruin_frac_min_ * num_clients));
    int k_max = std::max(k_min, static_cast<int>(ruin_frac_max_ * num_clients));
    std::uniform_int_distribution<int> k_dist(k_min, k_max);
    int k = k_dist(rng_);

    // Collect all assigned clients to pick a seed from.
    std::vector<int> assigned;
    assigned.reserve(num_clients);
    for (int c = 0; c < num_clients; ++c) {
        if (sol.is_assigned(c))
            assigned.push_back(c);
    }

    if (assigned.empty())
        return {};

    // Pick a random seed client.
    std::uniform_int_distribution<int> seed_dist(
        0, static_cast<int>(assigned.size()) - 1);
    int seed_client = assigned[seed_dist(rng_)];

    // Find the k-1 nearest assigned clients to the seed (plus the seed itself).
    // Sort assigned clients by distance to seed node.
    int seed_node = data_->num_depots() + seed_client;

    std::vector<std::pair<int, int>> dists;  // (distance, client)
    dists.reserve(assigned.size());
    for (int c : assigned) {
        int node = data_->num_depots() + c;
        int d = data_->dist(seed_node, node);
        dists.push_back({d, c});
    }

    // Partial sort to get the k nearest.
    k = std::min(k, static_cast<int>(dists.size()));
    std::partial_sort(dists.begin(), dists.begin() + k, dists.end());

    // Collect clients to remove.
    std::vector<int> removed;
    removed.reserve(k);
    for (int i = 0; i < k; ++i) {
        removed.push_back(dists[i].second);
    }

    // Remove them from the solution.
    // We must be careful: removing changes positions.  Process by route,
    // removing in reverse position order to keep indices stable.
    for (int v = 0; v < sol.num_routes(); ++v) {
        auto const& route = sol.route(v);
        // Find positions of clients to remove in this route.
        std::vector<int> positions;
        for (int pos = 0; pos < route.size(); ++pos) {
            int c = route.client(pos);
            for (int r : removed) {
                if (c == r) {
                    positions.push_back(pos);
                    break;
                }
            }
        }
        // Remove in reverse order to keep positions stable.
        std::sort(positions.rbegin(), positions.rend());
        for (int pos : positions) {
            sol.remove_client(v, pos);
        }
    }

    return removed;
}

void IteratedLocalSearch::recreate_(Solution& sol,
                                     std::vector<int> const& removed,
                                     CostEvaluator const& eval)
{
    // Greedy insertion: for each removed client, find the cheapest
    // (vehicle, position) and insert it there.
    // Process in random order for diversity.
    std::vector<int> order = removed;
    std::shuffle(order.begin(), order.end(), rng_);

    for (int client : order) {
        int best_vehicle = -1;
        int best_pos = -1;
        int64_t best_delta = std::numeric_limits<int64_t>::max();

        for (int v = 0; v < sol.num_routes(); ++v) {
            auto const& route = sol.route(v);
            int route_size = route.size();

            for (int pos = 0; pos <= route_size; ++pos) {
                int64_t delta = eval.eval_insert_cost(route, pos, client);
                if (delta < best_delta) {
                    best_delta = delta;
                    best_vehicle = v;
                    best_pos = pos;
                }
            }
        }

        if (best_vehicle >= 0) {
            sol.insert_client(best_vehicle, best_pos, client);
        }
    }
}

} // namespace coso
