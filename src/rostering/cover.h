#pragma once

#include "rostering/rostering_data.h"

#include <algorithm>
#include <climits>
#include <compare>
#include <cstdint>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace coso {

/// Cover crediting for one (shift, day) cell (#255).
///
/// INRC-II assigns a nurse to a (shift, skill) pair, so an employee counts
/// toward one skill demand on a cell, not toward every one they qualify for.
/// The schedule stores only the shift, so the cell's workers are credited to
/// its demands in whichever way prices the cell's cover cheapest -- hard
/// cover units first, then soft cost (see CoverCost).  Since the
/// skill of an assignment affects nothing but cover, that is the same as
/// optimising over the skill of every assignment.
///
/// The rule, for a cell's workers and demands:
///   - a demand with no required_skill is a headcount: it counts every
///     worker, independently of the crediting below;
///   - a worker is credited to at most one skilled demand, and only to one
///     whose required_skill they hold;
///   - a worker holding a skill the cell has no demand for may be credited to
///     none (they work the cell under that skill); a worker whose every skill
///     is demanded on the cell is credited to exactly one, and a worker
///     holding none of the demanded skills to none.
///
/// With a single skilled demand on the cell, the first two rules reduce to
/// counting the workers holding its skill -- the engine's behaviour before
/// #255 -- except that a worker who also holds an undemanded skill is not
/// forced into over-cover.

namespace cover_detail {

[[nodiscard]] inline bool has_skill(RosteringData const& data, int e, std::string const& skill) {
    auto const& skills = data.employees[e].skills;
    return std::ranges::find(skills, skill) != skills.end();
}

}  // namespace cover_detail

/// Employees short of min_employees and over max_employees for a demand
/// credited `count` workers.
[[nodiscard]] inline std::pair<int, int> cover_units(RosteringData::Demand const& dem, int count) {
    int const under = std::max(0, dem.min_employees - count);
    int const over = dem.max_employees < INT_MAX ? std::max(0, count - dem.max_employees) : 0;
    return {under, over};
}

/// A cover cost, compared lexicographically: hard units first, so no soft
/// weight, however large, buys a hard violation; then soft cost; then plain
/// shortfall units, so a demand with under_weight 0 still draws a worker
/// that is free to cover it.
struct CoverCost {
    int64_t hard = 0;
    int64_t soft = 0;
    int64_t under = 0;

    friend CoverCost operator+(CoverCost a, CoverCost b) noexcept {
        return {a.hard + b.hard, a.soft + b.soft, a.under + b.under};
    }
    friend CoverCost operator-(CoverCost a, CoverCost b) noexcept {
        return {a.hard - b.hard, a.soft - b.soft, a.under - b.under};
    }
    friend auto operator<=>(CoverCost const&, CoverCost const&) = default;
};

/// Cost of `dem` credited `count` workers, at its declared weights; a hard
/// demand's units go to `hard`.
[[nodiscard]] inline CoverCost demand_cover_cost(RosteringData::Demand const& dem, int count) {
    auto const [under, over] = cover_units(dem, count);
    if (dem.hard) {
        return {.hard = under + over, .soft = 0, .under = under};
    }
    return {.hard = 0,
            .soft = static_cast<int64_t>(under) * dem.under_weight +
                    static_cast<int64_t>(over) * dem.over_weight,
            .under = under};
}

/// Cost of the k-th worker (k >= 1) credited to `dem`: the difference of
/// demand_cover_cost at k and k - 1.  Each component is a sum of
/// max(0, linear) terms with non-negative weights, so convex in k whatever
/// min_employees and max_employees are, and the marginal non-decreasing.
[[nodiscard]] inline CoverCost cover_marginal(RosteringData::Demand const& dem, int k) {
    return demand_cover_cost(dem, k) - demand_cover_cost(dem, k - 1);
}

/// Workers credited to each of `demands` (counts[j] for demands[j]) under the
/// rule above, minimising the sum over j of the costs marginal(j, 1..counts[j]).
///
/// `marginal(j, k)` is the cost of the k-th worker credited to demands[j],
/// of any totally ordered additive type whose value-initialised object is
/// zero, and must be non-decreasing in k: the per-demand cost is then convex and
/// the crediting is a min-cost flow, solved by successive shortest paths.
/// Every residual arc between demands costs 0 (moving a credited worker to
/// another demand they qualify for), so each worker's shortest augmenting
/// path is a BFS over the demands, ending at the cheapest reachable next
/// unit, or at no credit for a worker able to go uncredited.
template <class Marginal>
[[nodiscard]] std::vector<int> credit_cover(RosteringData const& data,
                                            std::span<RosteringData::Demand const> demands,
                                            std::span<int const> workers, Marginal&& marginal) {
    int const m = static_cast<int>(demands.size());
    int const nw = static_cast<int>(workers.size());
    int const none = m;  // Node: credited to no demand.
    using Cost = std::invoke_result_t<Marginal&, int, int>;
    std::vector<int> counts(static_cast<size_t>(m), 0);

    // Eligible skilled demands per worker, and whether they may go uncredited.
    std::vector<std::vector<int>> eligible(static_cast<size_t>(nw));
    std::vector<char> may_skip(static_cast<size_t>(nw), 0);
    for (int w = 0; w < nw; ++w) {
        int const e = workers[w];
        for (int j = 0; j < m; ++j) {
            if (demands[j].required_skill.empty()) {
                ++counts[j];  // Headcount.
            } else if (cover_detail::has_skill(data, e, demands[j].required_skill)) {
                eligible[w].push_back(j);
            }
        }
        for (auto const& skill : data.employees[e].skills) {
            bool const demanded = std::ranges::any_of(
                demands, [&](RosteringData::Demand const& d) { return d.required_skill == skill; });
            if (!demanded) {
                may_skip[w] = 1;
                break;
            }
        }
    }

    std::vector<int> credited(static_cast<size_t>(nw), -1);  // Demand, `none`, or -1 unrouted.
    std::vector<int> via(static_cast<size_t>(m + 1));        // Worker moved into the node.
    std::vector<int> from(static_cast<size_t>(m + 1));       // Node that worker left; -1 = start.
    std::vector<char> seen(static_cast<size_t>(m + 1));
    std::vector<int> queue;
    queue.reserve(static_cast<size_t>(m));

    for (int w = 0; w < nw; ++w) {
        if (eligible[w].empty()) {
            continue;  // Holds no demanded skill: credited to nothing.
        }
        std::ranges::fill(seen, 0);
        queue.clear();
        auto reach = [&](int node, int worker, int prev) {
            if (!seen[node]) {
                seen[node] = 1;
                via[node] = worker;
                from[node] = prev;
                if (node != none) {
                    queue.push_back(node);
                }
            }
        };
        for (int j : eligible[w]) {
            reach(j, w, -1);
        }
        if (may_skip[w]) {
            reach(none, w, -1);
        }
        for (size_t q = 0; q < queue.size(); ++q) {
            int const j = queue[q];
            for (int k = 0; k < nw; ++k) {
                if (credited[k] != j) {
                    continue;
                }
                for (int j2 : eligible[k]) {
                    reach(j2, k, j);
                }
                if (may_skip[k]) {
                    reach(none, k, j);
                }
            }
        }

        // Cheapest end: the next unit of a reachable demand, or no credit.
        int best = -1;
        Cost best_cost{};
        for (int j = 0; j <= m; ++j) {
            if (!seen[j]) {
                continue;
            }
            Cost const cost = j == none ? Cost{} : marginal(j, counts[j] + 1);
            if (best < 0 || cost < best_cost) {
                best = j;
                best_cost = cost;
            }
        }
        if (best != none) {
            ++counts[best];
        }
        for (int node = best; node >= 0;) {
            int const worker = via[node];
            int const prev = from[node];
            credited[worker] = node;
            node = prev;
        }
    }
    return counts;
}

/// credit_cover at the demands' declared weights, hard units first (see
/// CoverCost) -- the crediting every engine component uses.
[[nodiscard]] inline std::vector<int> credit_cover(RosteringData const& data,
                                                   std::span<RosteringData::Demand const> demands,
                                                   std::span<int const> workers) {
    return credit_cover(data, demands, workers,
                        [&](int j, int k) { return cover_marginal(demands[j], k); });
}

/// (soft, hard) cover cost of cell (s, d) worked by `workers`: soft demands
/// at their declared weights, hard ones at `hard_weight` per employee short
/// or over.  Zero for a cell with no demand.
[[nodiscard]] inline std::pair<int, int> cell_cover_cost(RosteringData const& data, int s, int d,
                                                         std::span<int const> workers,
                                                         int hard_weight = 10000) {
    auto const demands = data.get_demands(s, d);
    if (demands.empty()) {
        return {0, 0};
    }
    auto const counts = credit_cover(data, demands, workers);
    int soft = 0;
    int hard = 0;
    for (size_t j = 0; j < demands.size(); ++j) {
        auto const& dem = demands[j];
        auto [under, over] = cover_units(dem, counts[j]);
        if (dem.hard) {
            hard += (under + over) * hard_weight;
        } else {
            soft += under * dem.under_weight + over * dem.over_weight;
        }
    }
    return {soft, hard};
}

/// Whether employee e can be credited to `dem`: it has no required skill,
/// or e holds it.
[[nodiscard]] inline bool qualifies(RosteringData const& data, int e,
                                    RosteringData::Demand const& dem) {
    return dem.required_skill.empty() || cover_detail::has_skill(data, e, dem.required_skill);
}

/// Employees assigned shift s on day d.
[[nodiscard]] inline std::vector<int> cell_workers(std::vector<std::vector<int>> const& schedule,
                                                   int s, int d) {
    std::vector<int> workers;
    for (int e = 0; e < static_cast<int>(schedule.size()); ++e) {
        if (schedule[e][d] == s) {
            workers.push_back(e);
        }
    }
    return workers;
}

/// Workers credited to the j-th demand of cell (s, d) under `schedule`.
[[nodiscard]] inline int credited_count(RosteringData const& data,
                                        std::vector<std::vector<int>> const& schedule, int s, int d,
                                        size_t j) {
    auto const workers = cell_workers(schedule, s, d);
    return credit_cover(data, data.get_demands(s, d), workers)[j];
}

}  // namespace coso
