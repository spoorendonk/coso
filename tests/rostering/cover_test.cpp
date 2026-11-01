#include "rostering/cover.h"

#include "rostering/constraints/constraint.h"
#include "rostering/cost_evaluator.h"
#include "rostering/rostering_data.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <climits>
#include <random>
#include <string>
#include <vector>

using namespace coso;

namespace {

int cell_cost(std::vector<RosteringData::Demand> const& demands, std::vector<int> const& counts) {
    int cost = 0;
    for (size_t j = 0; j < demands.size(); ++j) {
        auto [under, over] = cover_units(demands[j], counts[j]);
        cost += under * demands[j].under_weight + over * demands[j].over_weight;
    }
    return cost;
}

/// The cheapest cover over every crediting the rule in rostering/cover.h
/// allows, by enumeration.
int brute_force_cost(RosteringData const& data, std::vector<RosteringData::Demand> const& demands,
                     std::vector<int> const& workers) {
    int const m = static_cast<int>(demands.size());
    std::vector<std::vector<int>> options;  // Demand index, or -1 for none.
    std::vector<int> base(static_cast<size_t>(m), 0);
    for (int e : workers) {
        std::vector<int> opts;
        bool undemanded_skill = false;
        for (auto const& skill : data.employees[e].skills) {
            if (std::ranges::none_of(demands,
                                     [&](auto const& d) { return d.required_skill == skill; })) {
                undemanded_skill = true;
            }
        }
        for (int j = 0; j < m; ++j) {
            if (demands[j].required_skill.empty()) {
                ++base[j];
            } else if (std::ranges::find(data.employees[e].skills, demands[j].required_skill) !=
                       data.employees[e].skills.end()) {
                opts.push_back(j);
            }
        }
        if (opts.empty() || undemanded_skill) {
            opts.push_back(-1);
        }
        options.push_back(opts);
    }
    int best = INT_MAX;
    std::vector<size_t> pick(options.size(), 0);
    while (true) {
        auto counts = base;
        for (size_t w = 0; w < options.size(); ++w) {
            if (options[w][pick[w]] >= 0) {
                ++counts[options[w][pick[w]]];
            }
        }
        best = std::min(best, cell_cost(demands, counts));
        size_t w = 0;
        while (w < pick.size() && ++pick[w] == options[w].size()) {
            pick[w++] = 0;
        }
        if (w == pick.size()) {
            break;
        }
    }
    return best;
}

}  // namespace

TEST_CASE("credit_cover matches enumeration on random cells", "[rostering][cover]") {
    std::mt19937 rng(255);
    std::vector<std::string> const skills = {"A", "B", "C", "D"};
    for (int trial = 0; trial < 2000; ++trial) {
        INFO("trial " << trial);
        RosteringData data;
        int const ne = std::uniform_int_distribution<int>(1, 6)(rng);
        for (int e = 0; e < ne; ++e) {
            RosteringData::Employee emp{.name = "e" + std::to_string(e)};
            for (auto const& s : skills) {
                if (rng() % 2 == 0) {
                    emp.skills.push_back(s);
                }
            }
            data.employees.push_back(emp);
        }
        std::vector<RosteringData::Demand> demands;
        // Up to one demand per skill, plus maybe a skill-free headcount.
        for (auto const& s : skills) {
            if (rng() % 3 != 0) {
                int const lo = std::uniform_int_distribution<int>(0, 3)(rng);
                int const hi =
                    rng() % 3 == 0 ? INT_MAX : lo + std::uniform_int_distribution<int>(0, 2)(rng);
                demands.push_back({.min_employees = lo,
                                   .max_employees = hi,
                                   .required_skill = s,
                                   .under_weight = std::uniform_int_distribution<int>(0, 9)(rng),
                                   .over_weight = std::uniform_int_distribution<int>(0, 9)(rng)});
            }
        }
        if (rng() % 3 == 0) {
            demands.push_back({.min_employees = 2, .max_employees = 3});
        }
        std::vector<int> workers(static_cast<size_t>(ne));
        for (int e = 0; e < ne; ++e) {
            workers[e] = e;
        }
        std::ranges::shuffle(workers, rng);

        auto const counts = credit_cover(data, demands, workers);
        CHECK(cell_cost(demands, counts) == brute_force_cost(data, demands, workers));
    }
}

TEST_CASE("credit_cover credits a worker to one skill demand", "[rostering][cover]") {
    RosteringData data;
    data.employees = {{.name = "Cat", .skills = {"H", "N"}}};
    std::vector<RosteringData::Demand> const demands = {
        {.min_employees = 1, .required_skill = "H", .under_weight = 5},
        {.min_employees = 1, .required_skill = "N", .under_weight = 9},
    };
    std::vector<int> const workers = {0};
    CHECK(credit_cover(data, demands, workers) == std::vector<int>{0, 1});
}

TEST_CASE("credit_cover lets a worker with an undemanded skill avoid over-cover",
          "[rostering][cover]") {
    RosteringData data;
    data.employees = {{.name = "Ann", .skills = {"H"}}, {.name = "Bob", .skills = {"H", "N"}}};
    std::vector<RosteringData::Demand> const demands = {
        {.min_employees = 1, .max_employees = 1, .required_skill = "H"},
    };
    std::vector<int> const workers = {0, 1};
    // Bob works the cell as an N, which the cell does not demand.
    CHECK(credit_cover(data, demands, workers) == std::vector<int>{1});

    // Without that skill he must count, and the cell is over.
    data.employees[1].skills = {"H"};
    CHECK(credit_cover(data, demands, workers) == std::vector<int>{2});
}

TEST_CASE("cover cost prices every demand on a multi-skill cell", "[rostering][cover]") {
    RosteringData data;
    data.shift_types = {{.name = "N"}};
    data.employees = {{.name = "Ann", .skills = {"H"}},
                      {.name = "Ben", .skills = {"N"}},
                      {.name = "Cat", .skills = {"H", "N"}}};
    data.horizon = 1;
    REQUIRE(data.add_demand(0, 0, {.min_employees = 2, .required_skill = "H", .under_weight = 7}));
    REQUIRE(data.add_demand(0, 0, {.min_employees = 2, .required_skill = "N", .under_weight = 3}));
    REQUIRE(data.add_demand(0, 0, {.min_employees = 1, .required_skill = "X", .hard = true}));
    CHECK_FALSE(data.add_demand(0, 0, {.required_skill = "N"}));
    REQUIRE(data.get_demands(0, 0).size() == 3);

    RosteringCostEvaluator const eval(data);
    // Ann and Ben: one short on each skill demand, and nobody holds X.
    std::vector<std::vector<int>> schedule = {{0}, {0}, {-1}};
    CHECK(eval.demand_cost(schedule) == 7 + 3);
    CHECK(eval.hard_cover_cost(schedule) == eval.weights().hard_violation);
    // Cat joins and covers the costlier H shortfall.
    schedule[2][0] = 0;
    CHECK(eval.demand_cost(schedule) == 3);

    // DemandConstraint's delta agrees with its full evaluation.
    DemandConstraint const dc;
    std::vector<std::vector<int>> without = {{0}, {0}, {-1}};
    RosteringMove const move{.employee = 2, .day = 0, .old_shift = -1, .new_shift = 0};
    CHECK(dc.evaluate_delta(data, without, move) ==
          dc.evaluate(data, schedule) - dc.evaluate(data, without));
}
