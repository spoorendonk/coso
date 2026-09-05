#include "model/assignment_model.h"

#include "assignment/assignment_data.h"
#include "assignment/assignment_solution.h"
#include "assignment/construction.h"
#include "assignment/cost_evaluator.h"
#include "assignment/operators/block_swap.h"
#include "assignment/operators/pillar_move.h"
#include "assignment/operators/shift_move.h"
#include "assignment/operators/shift_swap.h"
#include "common/work_units.h"
#include "search/stop_criterion.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace coso {

// ---------------------------------------------------------------------------
//  Shift types & employees
// ---------------------------------------------------------------------------

int AssignmentModel::add_shift_type(ShiftTypeParams p) {
    int idx = static_cast<int>(shift_types_.size());
    shift_types_.push_back(std::move(p));
    return idx;
}

int AssignmentModel::add_employee(EmployeeParams p) {
    int idx = static_cast<int>(employees_.size());
    employees_.push_back(std::move(p));
    return idx;
}

// ---------------------------------------------------------------------------
//  Planning horizon
// ---------------------------------------------------------------------------

void AssignmentModel::set_horizon(int days) {
    horizon_ = days;
}

// ---------------------------------------------------------------------------
//  Demand
// ---------------------------------------------------------------------------

void AssignmentModel::add_demand(int shift_type, int day, DemandParams p) {
    demands_.push_back({shift_type, day, std::move(p)});
}

void AssignmentModel::add_demand(int shift_type, DemandParams p) {
    demands_all_.push_back({shift_type, std::move(p)});
}

// ---------------------------------------------------------------------------
//  Hard constraints
// ---------------------------------------------------------------------------

void AssignmentModel::set_max_consecutive_shifts(int n) {
    max_consecutive_shifts_ = n;
}

void AssignmentModel::set_min_rest_between_shifts(int hours) {
    min_rest_between_shifts_ = hours;
}

void AssignmentModel::add_forbidden_sequence(const std::vector<int>& shift_types) {
    forbidden_sequences_.push_back(shift_types);
}

// ---------------------------------------------------------------------------
//  Soft constraints
// ---------------------------------------------------------------------------

void AssignmentModel::add_preference(int employee, int day, int shift_type, int weight) {
    preferences_.push_back({employee, day, shift_type, weight});
}

void AssignmentModel::add_unavailability(int employee, int day) {
    unavailabilities_.push_back({employee, day});
}

// ---------------------------------------------------------------------------
//  Replanning
// ---------------------------------------------------------------------------

void AssignmentModel::set_published_schedule(const std::vector<std::vector<int>>& schedule) {
    published_schedule_ = schedule;
}

void AssignmentModel::set_change_penalty(int penalty) {
    change_penalty_ = penalty;
}

// ---------------------------------------------------------------------------
//  solve()
// ---------------------------------------------------------------------------

Result AssignmentModel::solve(TimeLimit tl) {
    auto wall_start = std::chrono::steady_clock::now();
    WorkUnits work;
    StopCriterion stop(tl.seconds);
    stop.set_work_limit(&work, WorkUnits::ticks_from_units(tl.work_units));

    work.count(static_cast<uint64_t>(shift_types_.size()) +
               static_cast<uint64_t>(employees_.size()) + static_cast<uint64_t>(demands_.size()) +
               static_cast<uint64_t>(demands_all_.size()) +
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

    // Compile the declared state into an AssignmentData instance.
    AssignmentData data;

    // Shift types.
    data.shift_types.reserve(shift_types_.size());
    for (auto const& st : shift_types_) {
        data.shift_types.push_back({
            .name = st.name,
            .start_hour = st.start_hour,
            .end_hour = st.end_hour,
            .duration_hours = st.duration_hours,
        });
    }

    // Employees.
    data.employees.reserve(employees_.size());
    for (auto const& e : employees_) {
        data.employees.push_back({
            .name = e.name,
            .skills = e.skills,
            .max_hours_per_week = e.max_hours_per_week,
            .max_consecutive_days = e.max_consecutive_days,
            .min_rest_hours = e.min_rest_hours,
        });
    }

    // Horizon.
    data.horizon = horizon_;

    // Demand entries (specific day).
    for (auto const& d : demands_) {
        auto key = AssignmentData::demand_key(d.shift_type, d.day);
        data.demand[key] = {
            .min_employees = d.params.min_employees,
            .max_employees = d.params.max_employees,
            .required_skill = d.params.required_skill,
        };
    }

    // Demand entries (all days).
    for (auto const& d : demands_all_) {
        for (int day = 0; day < data.horizon; ++day) {
            auto key = AssignmentData::demand_key(d.shift_type, day);
            // Only set if not already set by a specific-day entry.
            if (data.demand.find(key) == data.demand.end()) {
                data.demand[key] = {
                    .min_employees = d.params.min_employees,
                    .max_employees = d.params.max_employees,
                    .required_skill = d.params.required_skill,
                };
            }
        }
    }

    // Hard constraints.
    data.max_consecutive_shifts = max_consecutive_shifts_;
    data.min_rest_between_shifts = min_rest_between_shifts_;
    data.forbidden_sequences = forbidden_sequences_;

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
        data.unavailabilities.insert(AssignmentData::unavail_key(u.employee, u.day));
    }

    // Replanning.
    data.published_schedule = published_schedule_;
    data.change_penalty = change_penalty_;
    work.count(static_cast<uint64_t>(data.num_employees()) +
               static_cast<uint64_t>(data.num_shift_types()) + static_cast<uint64_t>(data.horizon));

    // Validate: need at least one employee and one shift type.
    if (data.employees.empty() || data.shift_types.empty() || data.horizon <= 0) {
        return {};  // cannot solve without employees/shifts/horizon
    }

    AssignmentCostEvaluator evaluator(data);
    AssignmentSolution greedy = construct_greedy(data, evaluator);
    work.count(static_cast<uint64_t>(data.horizon) *
               static_cast<uint64_t>(std::max(1, data.num_shift_types())));

    auto best_schedule = greedy.schedule();

    if (!stop.should_stop()) {
        AssignmentSolution alt = construct_ffd(data, evaluator);
        work.count(static_cast<uint64_t>(data.horizon) *
                   static_cast<uint64_t>(std::max(1, data.num_employees())));
        if (alt.cost() < greedy.cost()) {
            best_schedule = alt.schedule();
        }
    }

    AssignmentSolution best(data, evaluator);
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
    result.assignments_.assign(static_cast<size_t>(data.horizon), {});
    for (int d = 0; d < data.horizon; ++d) {
        for (int e = 0; e < data.num_employees(); ++e) {
            int s = best.get(e, d);
            if (s < 0) {
                continue;
            }
            result.assignments_[d].push_back(Result::Assignment{
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
            for (auto const& a : result.assignments_[d]) {
                if (a.shift == s) {
                    ++count;
                }
            }
            if (count < dem.min_employees) {
                result.unassigned_.push_back(AssignmentData::demand_key(s, d));
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
