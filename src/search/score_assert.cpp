#include "search/score_assert.h"

#include "routing/resources/load_resource.h"

#include <cassert>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace coso::debug {

// ---------------------------------------------------------------------------
//  Route consistency
// ---------------------------------------------------------------------------

void assert_route_consistency(Route const& route, ProblemData const& data)
{
    int n = route.size();

    // Recompute load prefix from scratch.
    std::vector<LoadResource::State> prefix(n + 1);
    prefix[0] = LoadResource::init_depot(data);
    for (int i = 0; i < n; ++i) {
        auto cs = LoadResource::init(data, route.client(i));
        prefix[i + 1] = LoadResource::merge(prefix[i], cs);
    }

    // Recompute load suffix from scratch.
    std::vector<LoadResource::State> suffix(n + 1);
    suffix[n] = LoadResource::init_depot(data);
    for (int i = n - 1; i >= 0; --i) {
        auto cs = LoadResource::init(data, route.client(i));
        suffix[i] = LoadResource::merge(cs, suffix[i + 1]);
    }

    // Compare prefix arrays.
    for (int i = -1; i < n; ++i) {
        auto const& stored = route.load_prefix(i);
        auto const& recomp = prefix[i + 1];
        assert(stored.num_dims() == recomp.num_dims());
        for (int d = 0; d < stored.num_dims(); ++d) {
            assert(stored.dims[d].delivery == recomp.dims[d].delivery);
            assert(stored.dims[d].pickup == recomp.dims[d].pickup);
            assert(stored.dims[d].load == recomp.dims[d].load);
        }
    }

    // Compare suffix arrays.
    for (int i = 0; i <= n; ++i) {
        auto const& stored = route.load_suffix(i);
        auto const& recomp = suffix[i];
        assert(stored.num_dims() == recomp.num_dims());
        for (int d = 0; d < stored.num_dims(); ++d) {
            assert(stored.dims[d].delivery == recomp.dims[d].delivery);
            assert(stored.dims[d].pickup == recomp.dims[d].pickup);
            assert(stored.dims[d].load == recomp.dims[d].load);
        }
    }

    // Recompute load excess.
    int expected_excess = 0;
    if (n > 0) {
        expected_excess = LoadResource::excess(
            prefix[n], data.vehicle_type(route.vehicle_type()));
    }
    assert(route.load_excess() == expected_excess);

    // Recompute distance.
    int profile = data.vehicle_type(route.vehicle_type()).profile;
    int depot = 0;
    int expected_dist = 0;
    if (n > 0) {
        int first_node = data.num_depots() + route.client(0);
        int last_node  = data.num_depots() + route.client(n - 1);
        expected_dist += data.dist(profile, depot, first_node);
        for (int i = 0; i + 1 < n; ++i) {
            int from = data.num_depots() + route.client(i);
            int to   = data.num_depots() + route.client(i + 1);
            expected_dist += data.dist(profile, from, to);
        }
        expected_dist += data.dist(profile, last_node, depot);
    }
    assert(route.distance() == expected_dist);
}

// ---------------------------------------------------------------------------
//  Solution consistency
// ---------------------------------------------------------------------------

void assert_solution_consistency(Solution const& sol,
                                 CostEvaluator const& eval,
                                 ProblemData const& data)
{
    int num_clients = data.num_clients();

    // Track which clients appear in routes.
    std::vector<int> client_count(num_clients, 0);

    for (int v = 0; v < sol.num_routes(); ++v) {
        auto const& route = sol.route(v);

        // Verify route internal consistency.
        assert_route_consistency(route, data);

        // Count client appearances.
        for (int i = 0; i < route.size(); ++i) {
            int c = route.client(i);
            assert(c >= 0 && c < num_clients);
            client_count[c]++;
        }
    }

    // Each client must appear exactly once or be unassigned.
    for (int c = 0; c < num_clients; ++c) {
        assert(client_count[c] <= 1);

        if (client_count[c] == 1) {
            assert(sol.is_assigned(c));
        } else {
            assert(!sol.is_assigned(c));
        }
    }

    // Verify unassigned count matches.
    int expected_unassigned = 0;
    for (int c = 0; c < num_clients; ++c) {
        if (client_count[c] == 0)
            expected_unassigned++;
    }
    assert(sol.num_unassigned() == expected_unassigned);

    // Recompute total cost from scratch and compare.
    int64_t recomputed_cost = 0;
    for (int v = 0; v < sol.num_routes(); ++v) {
        recomputed_cost += eval.route_cost(sol.route(v));
    }
    assert(sol.cost(eval) == recomputed_cost);

    // Recompute objective and penalty separately.
    int64_t recomputed_obj = 0;
    int64_t recomputed_pen = 0;
    for (int v = 0; v < sol.num_routes(); ++v) {
        recomputed_obj += eval.route_objective(sol.route(v));
        recomputed_pen += eval.route_penalty(sol.route(v));
    }
    assert(sol.objective(eval) == recomputed_obj);
    assert(sol.penalty(eval) == recomputed_pen);
}

// ---------------------------------------------------------------------------
//  Cost delta
// ---------------------------------------------------------------------------

void assert_cost_delta(int64_t predicted, int64_t actual,
                       std::string_view context)
{
    if (predicted != actual) {
        std::ostringstream oss;
        oss << "Score corruption: predicted delta = " << predicted
            << ", actual delta = " << actual
            << ", difference = " << (predicted - actual);
        if (!context.empty())
            oss << " [" << context << "]";
        // In debug builds this will abort; the message is for diagnostics.
        assert(false && "score delta mismatch");
        // Fallback for NDEBUG builds where assert is compiled out:
        // unreachable in debug, but silences "unused variable" warnings.
        (void)oss;
    }
}

} // namespace coso::debug
