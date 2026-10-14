#pragma once

#include "model/types.h"

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace coso::e2e {

/// What two solves of one scenario say about work counting (#218). Inconclusive
/// means the counts matched but a solve ended before its budget, so they would
/// have matched under any larger budget; it fails the check like Nondeterministic does.
enum class Determinism : std::uint8_t {
    NotRequested,
    Deterministic,
    Nondeterministic,
    Inconclusive,
};

char const* to_string(Determinism d);

struct CheckReport {
    bool pass = true;
    bool feasible = true;
    bool nonnegative_cost = true;
    Determinism deterministic_work = Determinism::NotRequested;
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
