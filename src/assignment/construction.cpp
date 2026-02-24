#include "assignment/construction.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <numeric>
#include <vector>

namespace coso {

namespace {

/// Check whether assigning employee e to shift s on day d would violate
/// any hard constraint (unavailability, consecutive days, rest time).
bool is_feasible_assignment(AssignmentData const& data,
                            std::vector<std::vector<int>> const& schedule,
                            int e, int d, int s)
{
    // Unavailability.
    if (data.is_unavailable(e, d))
        return false;

    // Already assigned on this day.
    if (schedule[e][d] >= 0)
        return false;

    // Consecutive days limit.
    int max_consec = std::min(data.max_consecutive_shifts,
                              data.employees[e].max_consecutive_days);
    // Count the run of consecutive working days ending at d-1.
    int run_before = 0;
    for (int dd = d - 1; dd >= 0 && schedule[e][dd] >= 0; --dd)
        ++run_before;
    // Count the run of consecutive working days starting at d+1.
    int run_after = 0;
    for (int dd = d + 1; dd < data.horizon && schedule[e][dd] >= 0; ++dd)
        ++run_after;
    if (run_before + 1 + run_after > max_consec)
        return false;

    // Minimum rest between shifts.
    int min_rest = std::max(data.min_rest_between_shifts,
                            data.employees[e].min_rest_hours);
    if (min_rest > 0) {
        int ns = data.num_shift_types();
        // Check rest with previous day's shift.
        if (d > 0) {
            int prev = schedule[e][d - 1];
            if (prev >= 0 && prev < ns) {
                int end_prev  = data.shift_types[prev].end_hour;
                int start_cur = data.shift_types[s].start_hour;
                int rest      = (24 - end_prev) + start_cur;
                if (rest < min_rest)
                    return false;
            }
        }
        // Check rest with next day's shift.
        if (d + 1 < data.horizon) {
            int next = schedule[e][d + 1];
            if (next >= 0 && next < ns) {
                int end_cur    = data.shift_types[s].end_hour;
                int start_next = data.shift_types[next].start_hour;
                int rest       = (24 - end_cur) + start_next;
                if (rest < min_rest)
                    return false;
            }
        }
    }

    // Forbidden sequences: check if adding shift s on day d creates one.
    for (auto const& seq : data.forbidden_sequences) {
        int len = static_cast<int>(seq.size());
        if (len < 2)
            continue;
        // For each position k in the sequence where day d could contribute,
        // check if the full sequence would be matched.
        for (int k = 0; k < len; ++k) {
            int start_day = d - k;
            if (start_day < 0 || start_day + len > data.horizon)
                continue;
            bool match = true;
            for (int j = 0; j < len; ++j) {
                int dd = start_day + j;
                int shift_on_dd = (dd == d) ? s : schedule[e][dd];
                if (shift_on_dd != seq[j]) {
                    match = false;
                    break;
                }
            }
            if (match)
                return false;
        }
    }

    return true;
}

/// Check whether employee has a given skill (empty skill = always true).
bool employee_has_skill(AssignmentData const& data, int e,
                        std::string const& skill)
{
    if (skill.empty())
        return true;
    auto const& skills = data.employees[e].skills;
    return std::find(skills.begin(), skills.end(), skill) != skills.end();
}

} // anonymous namespace

// ---------------------------------------------------------------------------
//  First-Fit-Decreasing
// ---------------------------------------------------------------------------

AssignmentSolution construct_ffd(AssignmentData const& data,
                                 AssignmentCostEvaluator const& evaluator)
{
    AssignmentSolution sol(data, evaluator);

    // Build list of (shift_type, day, min_employees) demand entries.
    struct DemandEntry {
        int shift_type;
        int day;
        int min_employees;
        std::string required_skill;
    };

    std::vector<DemandEntry> demands;
    int const ns = data.num_shift_types();
    int const H  = data.horizon;

    for (int s = 0; s < ns; ++s) {
        for (int d = 0; d < H; ++d) {
            auto dem = data.get_demand(s, d);
            if (dem.min_employees > 0) {
                demands.push_back({s, d, dem.min_employees, dem.required_skill});
            }
        }
    }

    // Sort by required employees descending (FFD ordering).
    std::sort(demands.begin(), demands.end(),
              [](DemandEntry const& a, DemandEntry const& b) {
                  return a.min_employees > b.min_employees;
              });

    // For each demand entry, assign employees.
    for (auto const& dem : demands) {
        // Count how many are already assigned to this (shift, day).
        int assigned = 0;
        for (int e = 0; e < data.num_employees(); ++e) {
            if (sol.get(e, dem.day) == dem.shift_type
                && employee_has_skill(data, e, dem.required_skill)) {
                ++assigned;
            }
        }

        // Sort employees by assignment cost delta (cheapest first).
        std::vector<int> candidates(data.num_employees());
        std::iota(candidates.begin(), candidates.end(), 0);

        // Filter and sort: prefer employees that are feasible, then by cost.
        std::sort(candidates.begin(), candidates.end(),
                  [&](int a, int b) {
                      // Prefer employees with fewer total assigned shifts
                      // (load-balancing heuristic).
                      int load_a = 0, load_b = 0;
                      for (int dd = 0; dd < H; ++dd) {
                          if (sol.get(a, dd) >= 0) ++load_a;
                          if (sol.get(b, dd) >= 0) ++load_b;
                      }
                      return load_a < load_b;
                  });

        while (assigned < dem.min_employees) {
            bool found = false;
            for (int e : candidates) {
                if (!employee_has_skill(data, e, dem.required_skill))
                    continue;
                if (!is_feasible_assignment(data, sol.schedule(), e,
                                            dem.day, dem.shift_type))
                    continue;
                sol.assign(e, dem.day, dem.shift_type);
                ++assigned;
                found = true;
                break;
            }
            if (!found)
                break;  // No feasible employee available.
        }
    }

    return sol;
}

// ---------------------------------------------------------------------------
//  Greedy day-by-day
// ---------------------------------------------------------------------------

AssignmentSolution construct_greedy(AssignmentData const& data,
                                    AssignmentCostEvaluator const& evaluator)
{
    AssignmentSolution sol(data, evaluator);

    int const ns = data.num_shift_types();
    int const H  = data.horizon;
    int const ne = data.num_employees();

    // Iterate day by day, shift by shift.
    for (int d = 0; d < H; ++d) {
        for (int s = 0; s < ns; ++s) {
            auto dem = data.get_demand(s, d);
            if (dem.min_employees <= 0)
                continue;

            // Count currently assigned employees.
            int assigned = 0;
            for (int e = 0; e < ne; ++e) {
                if (sol.get(e, d) == s
                    && employee_has_skill(data, e, dem.required_skill)) {
                    ++assigned;
                }
            }

            // Sort employees by cost delta for this assignment.
            std::vector<int> candidates(ne);
            std::iota(candidates.begin(), candidates.end(), 0);
            std::sort(candidates.begin(), candidates.end(),
                      [&](int a, int b) {
                          // Sort by: fewest assigned days so far (load balance).
                          int load_a = 0, load_b = 0;
                          for (int dd = 0; dd < H; ++dd) {
                              if (sol.get(a, dd) >= 0) ++load_a;
                              if (sol.get(b, dd) >= 0) ++load_b;
                          }
                          return load_a < load_b;
                      });

            // Assign greedily until demand is met.
            while (assigned < dem.min_employees) {
                bool found = false;
                for (int e : candidates) {
                    if (!employee_has_skill(data, e, dem.required_skill))
                        continue;
                    if (!is_feasible_assignment(data, sol.schedule(), e, d, s))
                        continue;
                    sol.assign(e, d, s);
                    ++assigned;
                    found = true;
                    break;
                }
                if (!found)
                    break;
            }
        }
    }

    return sol;
}

} // namespace coso
