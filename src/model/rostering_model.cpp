#include "model/rostering_model.h"

#include "common/work_units.h"
#include "rostering/construction.h"
#include "rostering/cost_evaluator.h"
#include "rostering/operators/block_swap.h"
#include "rostering/operators/pillar_move.h"
#include "rostering/operators/shift_move.h"
#include "rostering/operators/shift_swap.h"
#include "rostering/rostering_data.h"
#include "rostering/rostering_solution.h"
#include "search/stop_criterion.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace coso {

// ---------------------------------------------------------------------------
//  Shift types & employees
// ---------------------------------------------------------------------------

int RosteringModel::add_shift_type(ShiftTypeParams p) {
    int idx = static_cast<int>(shift_types_.size());
    shift_types_.push_back(std::move(p));
    return idx;
}

int RosteringModel::add_employee(EmployeeParams p) {
    int idx = static_cast<int>(employees_.size());
    employees_.push_back(std::move(p));
    return idx;
}

// ---------------------------------------------------------------------------
//  Planning horizon
// ---------------------------------------------------------------------------

void RosteringModel::set_horizon(int days) {
    horizon_ = days;
}

// ---------------------------------------------------------------------------
//  Demand
// ---------------------------------------------------------------------------

void RosteringModel::add_demand(int shift_type, int day, DemandParams p) {
    demands_.push_back({shift_type, day, std::move(p)});
}

// ---------------------------------------------------------------------------
//  Hard constraints
// ---------------------------------------------------------------------------

void RosteringModel::add_forbidden_sequence(int first, int second) {
    forbidden_sequences_.emplace_back(first, second);
}

// ---------------------------------------------------------------------------
//  Soft constraints
// ---------------------------------------------------------------------------

void RosteringModel::add_preference(int employee, int day, int shift_type, int weight) {
    preferences_.push_back({employee, day, shift_type, weight});
}

void RosteringModel::add_unavailability(int employee, int day) {
    unavailabilities_.push_back({employee, day});
}

// ---------------------------------------------------------------------------
//  solve()
// ---------------------------------------------------------------------------

Result RosteringModel::solve(TimeLimit tl) {
    auto wall_start = std::chrono::steady_clock::now();
    WorkUnits work;
    StopCriterion stop(tl.seconds);
    stop.set_work_limit(&work, WorkUnits::ticks_from_units(tl.work_units));

    work.count(static_cast<uint64_t>(shift_types_.size()) +
               static_cast<uint64_t>(employees_.size()) + static_cast<uint64_t>(demands_.size()) +
               static_cast<uint64_t>(forbidden_sequences_.size()) +
               static_cast<uint64_t>(preferences_.size()) +
               static_cast<uint64_t>(unavailabilities_.size()) + 1);
    if (stop.should_stop()) {
        Result result;
        result.work_ticks_ = work.ticks();
        result.work_units_ = work.units();
        auto wall_end = std::chrono::steady_clock::now();
        result.elapsed_seconds_ = std::chrono::duration<double>(wall_end - wall_start).count();
        return result;
    }

    // Compile the declared state into a RosteringData instance.
    RosteringData data;

    // Shift types.
    data.shift_types.reserve(shift_types_.size());
    for (auto const& st : shift_types_) {
        data.shift_types.push_back({
            .name = st.name,
            .duration_minutes = st.duration_minutes,
        });
    }

    // Employees.
    data.employees.reserve(employees_.size());
    for (auto const& e : employees_) {
        data.employees.push_back({
            .name = e.name,
            .skills = e.skills,
            .max_consecutive_days = e.max_consecutive_days,
            .max_weekends = e.max_weekends,
            .min_total_minutes = e.min_total_minutes,
            .max_total_minutes = e.max_total_minutes,
        });
    }

    // Horizon.
    data.horizon = horizon_;

    // Demand entries (specific day).
    for (auto const& d : demands_) {
        auto key = RosteringData::demand_key(d.shift_type, d.day);
        data.demand[key] = {
            .min_employees = d.params.min_employees,
            .max_employees = d.params.max_employees,
            .required_skill = d.params.required_skill,
        };
    }

    // Hard constraints.
    data.forbidden_sequences.reserve(forbidden_sequences_.size());
    for (auto const& [first, second] : forbidden_sequences_) {
        data.forbidden_sequences.push_back({first, second});
    }

    // Preferences.
    data.preferences.reserve(preferences_.size());
    for (auto const& p : preferences_) {
        data.preferences.push_back({
            .employee = p.employee,
            .day = p.day,
            .shift_type = p.shift_type,
            .weight = p.weight,
        });
    }

    // Unavailabilities.
    for (auto const& u : unavailabilities_) {
        data.unavailabilities.insert(RosteringData::unavail_key(u.employee, u.day));
    }

    work.count(static_cast<uint64_t>(data.num_employees()) +
               static_cast<uint64_t>(data.num_shift_types()) + static_cast<uint64_t>(data.horizon));

    // Validate: need at least one employee and one shift type.
    if (data.employees.empty() || data.shift_types.empty() || data.horizon <= 0) {
        return {};  // cannot solve without employees/shifts/horizon
    }

    RosteringCostEvaluator evaluator(data);
    RosteringSolution greedy = construct_greedy(data, evaluator);
    work.count(static_cast<uint64_t>(data.horizon) *
               static_cast<uint64_t>(std::max(1, data.num_shift_types())));

    auto best_schedule = greedy.schedule();

    if (!stop.should_stop()) {
        RosteringSolution alt = construct_ffd(data, evaluator);
        work.count(static_cast<uint64_t>(data.horizon) *
                   static_cast<uint64_t>(std::max(1, data.num_employees())));
        if (alt.cost() < greedy.cost()) {
            best_schedule = alt.schedule();
        }
    }

    RosteringSolution best(data, evaluator);
    for (int e = 0; e < data.num_employees(); ++e) {
        for (int d = 0; d < data.horizon; ++d) {
            int s = best_schedule[e][d];
            if (s >= 0) {
                best.assign(e, d, s);
            }
        }
    }

    int iterations = 0;
    ShiftMove shift_move;
    ShiftSwap shift_swap;
    BlockSwap block_swap;

    while (!stop.should_stop()) {
        bool improved = false;

        if (shift_move.find_best_move(best)) {
            shift_move.apply(best);
            improved = true;
            work.count(3);
        }
        if (!stop.should_stop() && shift_swap.find_best_move(best)) {
            shift_swap.apply(best);
            improved = true;
            work.count(3);
        }
        if (!stop.should_stop() && block_swap.find_best_move(best)) {
            block_swap.apply(best);
            improved = true;
            work.count(3);
        }
        if (!stop.should_stop()) {
            int delta = pillar_vnd(best, 4, 2);
            if (delta < 0) {
                improved = true;
                work.count(3);
            }
        }

        if (!improved) {
            break;
        }
        ++iterations;
        work.count(1);
    }

    Result result;
    result.feasible_ = best.is_feasible();
    result.cost_ = static_cast<double>(best.cost());
    result.iterations_ = iterations;
    result.roster_.assign(static_cast<size_t>(data.horizon), {});
    for (int d = 0; d < data.horizon; ++d) {
        for (int e = 0; e < data.num_employees(); ++e) {
            int s = best.get(e, d);
            if (s < 0) {
                continue;
            }
            result.roster_[d].push_back(Result::RosterEntry{
                .employee = e,
                .shift = s,
                .employee_name = data.employees[e].name,
                .shift_name = data.shift_types[s].name,
            });
        }
    }

    // Track unmet minimum demand entries as encoded keys (shift,day).
    int const ns = data.num_shift_types();
    for (int s = 0; s < ns; ++s) {
        for (int d = 0; d < data.horizon; ++d) {
            auto dem = data.get_demand(s, d);
            int count = 0;
            for (auto const& a : result.roster_[d]) {
                if (a.shift == s) {
                    ++count;
                }
            }
            if (count < dem.min_employees) {
                result.unassigned_.push_back(RosteringData::demand_key(s, d));
            }
        }
    }

    result.work_ticks_ = work.ticks();
    result.work_units_ = work.units();

    auto wall_end = std::chrono::steady_clock::now();
    result.elapsed_seconds_ = std::chrono::duration<double>(wall_end - wall_start).count();

    return result;
}

}  // namespace coso
