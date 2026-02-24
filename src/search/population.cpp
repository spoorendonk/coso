#include "search/population.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <numeric>
#include <utility>

namespace coso {

Population::Population(ProblemData const& data,
                       int max_feasible,
                       int max_infeasible,
                       int num_close)
    : data_(&data)
    , max_feasible_(max_feasible)
    , max_infeasible_(max_infeasible)
    , num_close_(num_close)
{
    assert(max_feasible_ > 0);
    assert(max_infeasible_ > 0);
}

void Population::add(Solution sol, CostEvaluator const& eval)
{
    auto cost = sol.cost(eval);
    auto edges = extract_edges(sol);
    bool feas = sol.feasible();

    Individual ind{std::move(sol), cost, std::move(edges)};

    SubPop& pop = feas ? feasible_ : infeasible_;
    int max_size = feas ? max_feasible_ : max_infeasible_;

    pop.push_back(std::move(ind));

    if (static_cast<int>(pop.size()) > max_size)
        survivor_selection(pop, max_size);
}

Solution const& Population::select_parent(std::mt19937& rng) const
{
    assert(size() > 0);
    return tournament(rng);
}

Solution const& Population::best_feasible() const
{
    assert(has_feasible());
    auto it = std::min_element(
        feasible_.begin(), feasible_.end(),
        [](Individual const& a, Individual const& b) {
            return a.cost < b.cost;
        });
    return it->sol;
}

// --------------------------------------------------------------------------- //
//  Edge extraction and broken-pairs diversity                                  //
// --------------------------------------------------------------------------- //

std::vector<std::pair<int, int>>
Population::extract_edges(Solution const& sol)
{
    std::vector<std::pair<int, int>> edges;

    for (auto const& route : sol.routes()) {
        auto clients = route.clients();
        for (int i = 0; i + 1 < static_cast<int>(clients.size()); ++i) {
            int a = clients[i];
            int b = clients[i + 1];
            if (a > b) std::swap(a, b);
            edges.emplace_back(a, b);
        }
    }

    std::sort(edges.begin(), edges.end());
    return edges;
}

int Population::broken_pairs(Individual const& a, Individual const& b)
{
    // Count symmetric difference of sorted edge sets.
    auto const& ea = a.edges;
    auto const& eb = b.edges;

    int diff = 0;
    size_t i = 0, j = 0;
    while (i < ea.size() && j < eb.size()) {
        if (ea[i] < eb[j]) {
            ++diff;
            ++i;
        } else if (eb[j] < ea[i]) {
            ++diff;
            ++j;
        } else {
            ++i;
            ++j;
        }
    }
    diff += static_cast<int>(ea.size() - i);
    diff += static_cast<int>(eb.size() - j);
    return diff;
}

// --------------------------------------------------------------------------- //
//  Biased fitness                                                              //
// --------------------------------------------------------------------------- //

std::vector<double>
Population::biased_fitness(SubPop const& pop) const
{
    int n = static_cast<int>(pop.size());
    if (n == 0) return {};

    // Rank by cost (0 = best).
    std::vector<int> cost_order(n);
    std::iota(cost_order.begin(), cost_order.end(), 0);
    std::sort(cost_order.begin(), cost_order.end(),
              [&](int a, int b) { return pop[a].cost < pop[b].cost; });

    std::vector<int> cost_rank(n);
    for (int r = 0; r < n; ++r)
        cost_rank[cost_order[r]] = r;

    // Diversity contribution: average broken-pairs distance to num_close
    // nearest neighbours in the sub-population.
    int nc = num_close_;
    if (nc <= 0)
        nc = std::max(1, n / 5);  // auto: 20% of pop size
    nc = std::min(nc, n - 1);     // can't exceed pop_size - 1

    std::vector<double> diversity(n, 0.0);

    if (nc > 0 && n > 1) {
        for (int i = 0; i < n; ++i) {
            // Compute broken-pairs distance to all others.
            std::vector<int> dists;
            dists.reserve(n - 1);
            for (int j = 0; j < n; ++j) {
                if (j != i)
                    dists.push_back(broken_pairs(pop[i], pop[j]));
            }
            // Average of nc closest.
            std::partial_sort(dists.begin(),
                              dists.begin() + nc,
                              dists.end());
            double sum = 0.0;
            for (int k = 0; k < nc; ++k)
                sum += dists[k];
            diversity[i] = sum / nc;
        }
    }

    // Rank by diversity (0 = most diverse = highest avg distance).
    std::vector<int> div_order(n);
    std::iota(div_order.begin(), div_order.end(), 0);
    std::sort(div_order.begin(), div_order.end(),
              [&](int a, int b) { return diversity[a] > diversity[b]; });

    std::vector<int> div_rank(n);
    for (int r = 0; r < n; ++r)
        div_rank[div_order[r]] = r;

    // Biased fitness = cost_rank + (1 - nc/n) * diversity_rank.
    double div_weight = 1.0 - static_cast<double>(nc) / n;
    std::vector<double> fitness(n);
    for (int i = 0; i < n; ++i)
        fitness[i] = cost_rank[i] + div_weight * div_rank[i];

    return fitness;
}

// --------------------------------------------------------------------------- //
//  Survivor selection                                                          //
// --------------------------------------------------------------------------- //

void Population::survivor_selection(SubPop& pop, int max_size)
{
    while (static_cast<int>(pop.size()) > max_size) {
        auto fitness = biased_fitness(pop);

        // Find worst (highest fitness).
        int worst = 0;
        for (int i = 1; i < static_cast<int>(pop.size()); ++i) {
            if (fitness[i] > fitness[worst])
                worst = i;
        }

        // Remove by swapping with last element.
        if (worst != static_cast<int>(pop.size()) - 1)
            pop[worst] = std::move(pop.back());
        pop.pop_back();
    }
}

// --------------------------------------------------------------------------- //
//  Parent selection (binary tournament)                                         //
// --------------------------------------------------------------------------- //

Solution const&
Population::tournament(std::mt19937& rng) const
{
    // Build combined fitness across both sub-populations.
    // We compute biased fitness within each sub-population independently,
    // then use those for tournament selection.
    auto feas_fit = biased_fitness(feasible_);
    auto infeas_fit = biased_fitness(infeasible_);

    int total = size();
    assert(total > 0);

    // Concatenate fitness values: feasible first, then infeasible.
    std::vector<double> all_fitness;
    all_fitness.reserve(total);
    all_fitness.insert(all_fitness.end(), feas_fit.begin(), feas_fit.end());
    all_fitness.insert(all_fitness.end(), infeas_fit.begin(), infeas_fit.end());

    std::uniform_int_distribution<int> dist(0, total - 1);
    int a = dist(rng);
    int b = dist(rng);

    // Break ties by choosing the first candidate.
    int winner = (all_fitness[a] <= all_fitness[b]) ? a : b;

    int fsize = feasible_size();
    if (winner < fsize)
        return feasible_[winner].sol;
    else
        return infeasible_[winner - fsize].sol;
}

} // namespace coso
