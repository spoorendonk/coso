#include "search/guided_local_search.h"

#include <cmath>

namespace coso {

GuidedLocalSearch::GuidedLocalSearch(ProblemData const& data, double lambda)
    : data_(&data),
      lambda_(lambda),
      num_nodes_(data.num_nodes()),
      penalties_(static_cast<size_t>(num_nodes_) * num_nodes_, 0) {}

void GuidedLocalSearch::penalize(Solution const& sol) {
    auto sol_edges = edges_(sol);

    if (sol_edges.empty()) {
        return;
    }

    // Find the edge with the highest utility.
    // utility(i,j) = d(i,j) / (1 + p(i,j))
    double best_utility = -1.0;
    int best_from = -1;
    int best_to = -1;

    for (auto [from, to] : sol_edges) {
        int d = data_->dist(from, to);
        int p = penalties_[from * num_nodes_ + to];
        double utility = static_cast<double>(d) / (1.0 + p);

        if (utility > best_utility) {
            best_utility = utility;
            best_from = from;
            best_to = to;
        }
    }

    if (best_from >= 0) {
        penalties_[best_from * num_nodes_ + best_to]++;
    }
}

int64_t GuidedLocalSearch::augmented_cost(Solution const& sol) const {
    auto sol_edges = edges_(sol);

    // Augmented cost = lambda * sum_{(i,j) in sol} d(i,j) * p(i,j)
    // Each penalty is weighted by the edge distance, following the standard
    // GLS formulation (Voudouris & Tsang 1999).
    double total = 0.0;
    for (auto [from, to] : sol_edges) {
        int p = penalties_[from * num_nodes_ + to];
        if (p > 0) {
            int d = data_->dist(from, to);
            total += static_cast<double>(d) * p;
        }
    }

    return static_cast<int64_t>(std::llround(lambda_ * total));
}

void GuidedLocalSearch::reset() {
    std::fill(penalties_.begin(), penalties_.end(), 0);
}

int GuidedLocalSearch::penalty(int from, int to) const {
    return penalties_[from * num_nodes_ + to];
}

std::vector<std::pair<int, int>> GuidedLocalSearch::edges_(Solution const& sol) const {
    std::vector<std::pair<int, int>> edges;
    int num_depots = data_->num_depots();

    for (int v = 0; v < sol.num_routes(); ++v) {
        auto const& route = sol.route(v);
        if (route.empty()) {
            continue;
        }

        int depot = 0;  // depot node index (first depot)

        // Edge from depot to first client.
        int first_node = num_depots + route.client(0);
        edges.emplace_back(depot, first_node);

        // Edges between consecutive clients.
        for (int i = 0; i + 1 < route.size(); ++i) {
            int from_node = num_depots + route.client(i);
            int to_node = num_depots + route.client(i + 1);
            edges.emplace_back(from_node, to_node);
        }

        // Edge from last client back to depot.
        int last_node = num_depots + route.client(route.size() - 1);
        edges.emplace_back(last_node, depot);
    }

    return edges;
}

}  // namespace coso
