#pragma once

#include "model/types.h"

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

Evaluation evaluate_checks(std::set<std::string> const& requested_checks,
                           SolveFn const& solve_once);

} // namespace coso::e2e
