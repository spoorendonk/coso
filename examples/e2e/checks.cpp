#include "checks.h"

#include <array>
#include <set>
#include <string>

namespace coso::e2e {
namespace {

bool is_known_check(std::string const& check_name)
{
    static constexpr std::array<char const*, 3> kKnownChecks = {
        "feasible",
        "nonnegative_cost",
        "deterministic_work",
    };
    for (char const* known : kKnownChecks) {
        if (check_name == known) {
            return true;
        }
    }
    return false;
}

bool requested(std::set<std::string> const& checks, std::string const& name)
{
    return checks.contains(name);
}

} // namespace

Evaluation evaluate_checks(std::set<std::string> const& requested_checks,
                           SolveFn const& solve_once)
{
    Evaluation eval{.result = solve_once(), .checks = {}};

    for (std::string const& check_name : requested_checks) {
        if (!is_known_check(check_name)) {
            eval.checks.pass = false;
            eval.checks.errors.push_back("unknown_check:" + check_name);
        }
    }

    if (requested(requested_checks, "feasible")) {
        eval.checks.feasible = eval.result.feasible();
        eval.checks.pass = eval.checks.pass && eval.checks.feasible;
    }

    if (requested(requested_checks, "nonnegative_cost")) {
        eval.checks.nonnegative_cost = (eval.result.cost() >= 0.0);
        eval.checks.pass = eval.checks.pass && eval.checks.nonnegative_cost;
    }

    if (requested(requested_checks, "deterministic_work")) {
        coso::Result const rerun = solve_once();
        eval.checks.deterministic_work =
            (eval.result.work_ticks() == rerun.work_ticks())
            && (eval.result.work_units() == rerun.work_units());
        eval.checks.pass = eval.checks.pass && eval.checks.deterministic_work;
    }

    return eval;
}

} // namespace coso::e2e
