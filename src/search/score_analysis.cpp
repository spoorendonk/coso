#include "search/score_analysis.h"

#include <format>
#include <sstream>

namespace coso {

SolutionAnalysis analyze(Solution const& sol,
                         CostEvaluator const& eval,
                         ProblemData const& data)
{
    SolutionAnalysis result;
    result.total_objective = 0;
    result.total_penalty   = 0;
    result.num_routes_used = 0;
    result.num_unserved    = sol.num_unassigned();
    result.feasible        = sol.feasible();

    int num_dims = data.num_load_dims();

    for (int r = 0; r < sol.num_routes(); ++r) {
        auto const& route = sol.route(r);
        if (route.empty())
            continue;

        ++result.num_routes_used;

        RouteAnalysis ra;
        ra.route_idx   = r;
        ra.vehicle_type = route.vehicle_type();
        ra.distance    = route.distance();
        ra.load_excess = route.load_excess();

        auto const& vt = data.vehicle_type(route.vehicle_type());
        ra.fixed_cost  = vt.cost.fixed_cost;
        ra.objective   = eval.route_objective(route);
        ra.penalty     = eval.route_penalty(route);

        // Client list.
        ra.clients.assign(route.clients().begin(), route.clients().end());

        // Total demand per dimension.
        ra.total_demand.assign(num_dims, 0);
        for (int i = 0; i < route.size(); ++i) {
            auto const& client = data.client(route.client(i));
            for (int d = 0; d < num_dims; ++d) {
                ra.total_demand[d] += client.demand[d];
            }
        }

        // Vehicle capacity.
        ra.capacity.assign(vt.capacity.begin(), vt.capacity.end());
        ra.capacity.resize(num_dims, 0);

        result.total_objective += ra.objective;
        result.total_penalty   += ra.penalty;
        result.routes.push_back(std::move(ra));
    }

    result.penalized_cost = result.total_objective + result.total_penalty;
    return result;
}

std::string SolutionAnalysis::to_string() const
{
    std::ostringstream os;

    os << std::format("=== Solution Analysis ===\n");
    os << std::format("  Routes used:    {}\n", num_routes_used);
    os << std::format("  Unserved:       {}\n", num_unserved);
    os << std::format("  Feasible:       {}\n", feasible ? "yes" : "no");
    os << std::format("  Objective:      {}\n", total_objective);
    os << std::format("  Penalty:        {}\n", total_penalty);
    os << std::format("  Penalized cost: {}\n", penalized_cost);
    os << '\n';

    for (auto const& ra : routes) {
        os << std::format("--- Route {} (vehicle type {}) ---\n",
                          ra.route_idx, ra.vehicle_type);
        os << "  Clients: [";
        for (size_t i = 0; i < ra.clients.size(); ++i) {
            if (i > 0) os << ", ";
            os << ra.clients[i];
        }
        os << "]\n";
        os << std::format("  Distance:    {}\n", ra.distance);
        os << std::format("  Fixed cost:  {}\n", ra.fixed_cost);
        os << std::format("  Objective:   {}\n", ra.objective);
        os << std::format("  Penalty:     {}\n", ra.penalty);
        os << std::format("  Load excess: {}\n", ra.load_excess);

        for (size_t d = 0; d < ra.total_demand.size(); ++d) {
            int cap = (d < ra.capacity.size()) ? ra.capacity[d] : 0;
            double pct = cap > 0
                             ? 100.0 * static_cast<double>(ra.total_demand[d])
                                   / static_cast<double>(cap)
                             : 0.0;
            os << std::format("  Dim {}: demand={} capacity={} ({:.1f}%)\n",
                              d, ra.total_demand[d], cap, pct);
        }
        os << '\n';
    }

    return os.str();
}

} // namespace coso
