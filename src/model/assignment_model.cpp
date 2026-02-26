#include "model/assignment_model.h"
#include "assignment/assignment_data.h"
#include "assignment/assignment_solution.h"
#include "common/work_units.h"
#include "search/stop_criterion.h"
#include "assignment/construction.h"
#include "assignment/cost_evaluator.h"
#include "assignment/operators/shift_move.h"
#include "assignment/operators/shift_swap.h"
#include "assignment/operators/block_swap.h"
#include "assignment/operators/pillar_move.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <map>
#include <mutex>
#include <stdexcept>

namespace coso {

// ---------------------------------------------------------------------------
//  Builder state storage
// ---------------------------------------------------------------------------
//
//  The AssignmentModel header has no private section, so we store builder
//  state in a file-local map keyed on the model address.  This avoids
//  changing the public header while still providing full state storage.
// ---------------------------------------------------------------------------

namespace {

struct BuilderData {
    std::vector<ShiftTypeParams>  shift_types;
    std::vector<EmployeeParams>   employees;
    int                           horizon = 0;

    // Demand entries: (shift_type, day) -> DemandParams.
    struct DemandEntry {
        int shift_type;
        int day;
        DemandParams params;
    };
    std::vector<DemandEntry> demands;

    // Demand for all days: shift_type -> DemandParams.
    struct DemandAllEntry {
        int shift_type;
        DemandParams params;
    };
    std::vector<DemandAllEntry> demands_all;

    int max_consecutive_shifts  = INT_MAX;
    int min_rest_between_shifts = 0;

    std::vector<std::vector<int>> forbidden_sequences;

    struct PrefEntry {
        int employee;
        int day;
        int shift_type;
        int weight;
    };
    std::vector<PrefEntry> preferences;

    struct UnavailEntry {
        int employee;
        int day;
    };
    std::vector<UnavailEntry> unavailabilities;

    std::vector<std::vector<int>> published_schedule;
    int change_penalty = 0;
};

// File-local registry of builder data keyed by model address.
std::mutex g_mtx;
std::map<const AssignmentModel*, BuilderData> g_data;

BuilderData& get_data(const AssignmentModel* m)
{
    std::lock_guard lk(g_mtx);
    return g_data[m];
}

void remove_data(const AssignmentModel* m)
{
    std::lock_guard lk(g_mtx);
    g_data.erase(m);
}

/// Compile builder data into an AssignmentData instance.
AssignmentData compile(const BuilderData& bd)
{
    AssignmentData data;

    // Shift types.
    data.shift_types.reserve(bd.shift_types.size());
    for (auto const& st : bd.shift_types) {
        data.shift_types.push_back({
            .name           = st.name,
            .start_hour     = st.start_hour,
            .end_hour       = st.end_hour,
            .duration_hours = st.duration_hours,
        });
    }

    // Employees.
    data.employees.reserve(bd.employees.size());
    for (auto const& e : bd.employees) {
        data.employees.push_back({
            .name                = e.name,
            .skills              = e.skills,
            .max_hours_per_week  = e.max_hours_per_week,
            .max_consecutive_days = e.max_consecutive_days,
            .min_rest_hours      = e.min_rest_hours,
        });
    }

    // Horizon.
    data.horizon = bd.horizon;

    // Demand entries (specific day).
    for (auto const& d : bd.demands) {
        auto key = AssignmentData::demand_key(d.shift_type, d.day);
        data.demand[key] = {
            .min_employees  = d.params.min_employees,
            .max_employees  = d.params.max_employees,
            .required_skill = d.params.required_skill,
        };
    }

    // Demand entries (all days).
    for (auto const& d : bd.demands_all) {
        for (int day = 0; day < data.horizon; ++day) {
            auto key = AssignmentData::demand_key(d.shift_type, day);
            // Only set if not already set by a specific-day entry.
            if (data.demand.find(key) == data.demand.end()) {
                data.demand[key] = {
                    .min_employees  = d.params.min_employees,
                    .max_employees  = d.params.max_employees,
                    .required_skill = d.params.required_skill,
                };
            }
        }
    }

    // Hard constraints.
    data.max_consecutive_shifts  = bd.max_consecutive_shifts;
    data.min_rest_between_shifts = bd.min_rest_between_shifts;
    data.forbidden_sequences     = bd.forbidden_sequences;

    // Preferences.
    data.preferences.reserve(bd.preferences.size());
    for (auto const& p : bd.preferences) {
        data.preferences.push_back({
            .employee   = p.employee,
            .day        = p.day,
            .shift_type = p.shift_type,
            .weight     = p.weight,
        });
    }

    // Unavailabilities.
    for (auto const& u : bd.unavailabilities) {
        data.unavailabilities.insert(
            AssignmentData::unavail_key(u.employee, u.day));
    }

    // Replanning.
    data.published_schedule = bd.published_schedule;
    data.change_penalty     = bd.change_penalty;

    return data;
}

} // anonymous namespace

AssignmentModel::~AssignmentModel()
{
    remove_data(this);
}

// ---------------------------------------------------------------------------
//  Shift types & employees
// ---------------------------------------------------------------------------

int AssignmentModel::add_shift_type(ShiftTypeParams p)
{
    auto& bd = get_data(this);
    int idx = static_cast<int>(bd.shift_types.size());
    bd.shift_types.push_back(std::move(p));
    return idx;
}

int AssignmentModel::add_employee(EmployeeParams p)
{
    auto& bd = get_data(this);
    int idx = static_cast<int>(bd.employees.size());
    bd.employees.push_back(std::move(p));
    return idx;
}

// ---------------------------------------------------------------------------
//  Planning horizon
// ---------------------------------------------------------------------------

void AssignmentModel::set_horizon(int days)
{
    get_data(this).horizon = days;
}

// ---------------------------------------------------------------------------
//  Demand
// ---------------------------------------------------------------------------

void AssignmentModel::add_demand(int shift_type, int day, DemandParams p)
{
    get_data(this).demands.push_back({shift_type, day, std::move(p)});
}

void AssignmentModel::add_demand(int shift_type, DemandParams p)
{
    get_data(this).demands_all.push_back({shift_type, std::move(p)});
}

// ---------------------------------------------------------------------------
//  Hard constraints
// ---------------------------------------------------------------------------

void AssignmentModel::set_max_consecutive_shifts(int n)
{
    get_data(this).max_consecutive_shifts = n;
}

void AssignmentModel::set_min_rest_between_shifts(int hours)
{
    get_data(this).min_rest_between_shifts = hours;
}

void AssignmentModel::add_forbidden_sequence(
    const std::vector<int>& shift_types)
{
    get_data(this).forbidden_sequences.push_back(shift_types);
}

// ---------------------------------------------------------------------------
//  Soft constraints
// ---------------------------------------------------------------------------

void AssignmentModel::add_preference(
    int employee, int day, int shift_type, int weight)
{
    get_data(this).preferences.push_back({employee, day, shift_type, weight});
}

void AssignmentModel::add_unavailability(int employee, int day)
{
    get_data(this).unavailabilities.push_back({employee, day});
}

// ---------------------------------------------------------------------------
//  Replanning
// ---------------------------------------------------------------------------

void AssignmentModel::set_published_schedule(
    const std::vector<std::vector<int>>& schedule)
{
    get_data(this).published_schedule = schedule;
}

void AssignmentModel::set_change_penalty(int penalty)
{
    get_data(this).change_penalty = penalty;
}

// ---------------------------------------------------------------------------
//  solve()
// ---------------------------------------------------------------------------

Result AssignmentModel::solve(TimeLimit tl)
{
    auto wall_start = std::chrono::steady_clock::now();
    WorkUnits work;
    StopCriterion stop(tl.seconds);
    stop.set_work_limit(&work, WorkUnits::ticks_from_units(tl.work_units));

    auto& bd = get_data(this);
    work.count(static_cast<uint64_t>(bd.shift_types.size())
             + static_cast<uint64_t>(bd.employees.size())
             + static_cast<uint64_t>(bd.demands.size())
             + static_cast<uint64_t>(bd.demands_all.size())
             + static_cast<uint64_t>(bd.forbidden_sequences.size())
             + static_cast<uint64_t>(bd.preferences.size())
             + static_cast<uint64_t>(bd.unavailabilities.size())
             + 1);
    if (stop.should_stop()) {
        Result result;
        result.work_ticks_ = work.ticks();
        result.work_units_ = work.units();
        auto wall_end = std::chrono::steady_clock::now();
        result.elapsed_seconds_ =
            std::chrono::duration<double>(wall_end - wall_start).count();
        return result;
    }

    // Compile the builder data into an AssignmentData instance.
    AssignmentData data = compile(bd);
    work.count(static_cast<uint64_t>(data.num_employees())
             + static_cast<uint64_t>(data.num_shift_types())
             + static_cast<uint64_t>(data.horizon));

    // Validate: need at least one employee and one shift type.
    if (data.employees.empty() || data.shift_types.empty()
        || data.horizon <= 0) {
        return {};  // cannot solve without employees/shifts/horizon
    }

    AssignmentCostEvaluator evaluator(data);
    AssignmentSolution greedy = construct_greedy(data, evaluator);
    work.count(static_cast<uint64_t>(data.horizon)
             * static_cast<uint64_t>(std::max(1, data.num_shift_types())));

    auto best_schedule = greedy.schedule();

    if (!stop.should_stop()) {
        AssignmentSolution alt = construct_ffd(data, evaluator);
        work.count(static_cast<uint64_t>(data.horizon)
                 * static_cast<uint64_t>(std::max(1, data.num_employees())));
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
            if (s < 0) continue;
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
                if (a.shift == s) ++count;
            }
            if (count < dem.min_employees) {
                result.unassigned_.push_back(AssignmentData::demand_key(s, d));
            }
        }
    }

    result.work_ticks_ = work.ticks();
    result.work_units_ = work.units();

    auto wall_end = std::chrono::steady_clock::now();
    result.elapsed_seconds_ =
        std::chrono::duration<double>(wall_end - wall_start).count();

    return result;
}

} // namespace coso
