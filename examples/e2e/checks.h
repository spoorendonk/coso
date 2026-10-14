#pragma once

#include "model/types.h"

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace coso::e2e {

struct CheckReport {
    bool pass = true;
    bool feasible = true;
    bool nonnegative_cost = true;
    bool deterministic_work = true;
    std::vector<std::string> errors;
};

struct Evaluation {
    coso::Result result;
    CheckReport checks;
};

using SolveFn = std::function<coso::Result()>;

/// `work_budget_ticks` is the scenario's work budget. deterministic_work passes only
/// when both solves reached it: a solve that exits its algorithm first would count
/// the same ticks under any budget, so matching counts there say nothing (#219).
Evaluation evaluate_checks(std::set<std::string> const& requested_checks,
                           std::uint64_t work_budget_ticks, SolveFn const& solve_once);

}  // namespace coso::e2e
