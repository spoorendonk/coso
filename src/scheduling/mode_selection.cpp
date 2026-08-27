#include "scheduling/mode_selection.h"

#include <algorithm>
#include <climits>
#include <numeric>

namespace coso {

// ---------------------------------------------------------------------------
//  ModeAssignment
// ---------------------------------------------------------------------------

ModeAssignment::ModeAssignment(ScheduleData const& data) {
    int n = data.num_operations();
    int nr = data.num_resources();

    modes_.resize(n);
    selected_mode_.assign(n, 0);

    // Build modes from the ScheduleData.
    // The base ScheduleData stores a single (duration, resource_usage) per
    // operation. We treat that as mode 0. If the operation has eligible
    // machines with different durations, each machine alternative becomes
    // an additional mode (with the same resource usage but different duration).
    for (int op = 0; op < n; ++op) {
        auto const& opdata = data.operation(op);

        // Collect base resource usage for this operation.
        std::vector<int> base_usage(nr, 0);
        for (int r = 0; r < nr; ++r) {
            base_usage[r] = data.resource_usage(op, r);
        }

        if (!opdata.eligible_machines.empty()) {
            // FJSP-style: each eligible machine is a mode with its own duration.
            for (int i = 0; i < static_cast<int>(opdata.eligible_machines.size()); ++i) {
                int dur = (i < static_cast<int>(opdata.durations_per_machine.size()))
                              ? opdata.durations_per_machine[i]
                              : opdata.duration;
                modes_[op].push_back({.duration = dur, .resource_usage = base_usage});
            }
        } else {
            // Single mode from fixed machine/duration.
            modes_[op].push_back({.duration = opdata.duration, .resource_usage = base_usage});
        }
    }
}

int ModeAssignment::total_resource_usage(int r) const {
    int total = 0;
    for (int op = 0; op < num_operations(); ++op) {
        total += resource_usage(op, r);
    }
    return total;
}

int ModeAssignment::total_duration() const {
    int total = 0;
    for (int op = 0; op < num_operations(); ++op) {
        total += duration(op);
    }
    return total;
}

bool ModeAssignment::mode_resource_feasible(ScheduleData const& data) const {
    int nr = data.num_resources();
    for (int op = 0; op < num_operations(); ++op) {
        for (int r = 0; r < nr; ++r) {
            if (resource_usage(op, r) > data.resource_capacity(r)) {
                return false;
            }
        }
    }
    return true;
}

void ModeAssignment::add_mode(int op, OperationMode m) {
    if (op >= static_cast<int>(modes_.size())) {
        modes_.resize(op + 1);
        selected_mode_.resize(op + 1, 0);
    }
    modes_[op].push_back(std::move(m));
}

// ---------------------------------------------------------------------------
//  Greedy mode selection
// ---------------------------------------------------------------------------

ModeAssignment greedy_mode_selection(ScheduleData const& data) {
    ModeAssignment assignment(data);
    int nr = data.num_resources();

    for (int op = 0; op < assignment.num_operations(); ++op) {
        int best_mode = 0;
        int best_cost = INT_MAX;

        for (int m = 0; m < assignment.num_modes(op); ++m) {
            auto const& mode = assignment.mode(op, m);

            // Check per-operation resource feasibility.
            bool feasible = true;
            for (int r = 0; r < nr; ++r) {
                int usage =
                    (r < static_cast<int>(mode.resource_usage.size())) ? mode.resource_usage[r] : 0;
                if (usage > data.resource_capacity(r)) {
                    feasible = false;
                    break;
                }
            }
            if (!feasible) {
                continue;
            }

            // Cost = sum of resource usage + duration (weighted equally).
            // This balances resource consumption against processing time.
            int cost = mode.duration;
            for (int r = 0; r < nr; ++r) {
                int usage =
                    (r < static_cast<int>(mode.resource_usage.size())) ? mode.resource_usage[r] : 0;
                cost += usage;
            }

            if (cost < best_cost) {
                best_cost = cost;
                best_mode = m;
            }
        }

        assignment.set_mode(op, best_mode);
    }

    return assignment;
}

// ---------------------------------------------------------------------------
//  Local search over mode assignments
// ---------------------------------------------------------------------------

void local_search_modes(ScheduleData const& data, ModeAssignment& assignment) {
    int nr = data.num_resources();
    bool improved = true;

    while (improved) {
        improved = false;

        // Steepest descent: find the best single-operation mode swap.
        int best_op = -1;
        int best_mode = -1;
        int best_delta = 0;  // negative = improvement

        for (int op = 0; op < assignment.num_operations(); ++op) {
            int current_mode = assignment.selected_mode(op);
            int current_dur = assignment.duration(op);

            for (int m = 0; m < assignment.num_modes(op); ++m) {
                if (m == current_mode) {
                    continue;
                }

                auto const& candidate = assignment.mode(op, m);

                // Check resource feasibility of the candidate mode.
                bool feasible = true;
                for (int r = 0; r < nr; ++r) {
                    int usage = (r < static_cast<int>(candidate.resource_usage.size()))
                                    ? candidate.resource_usage[r]
                                    : 0;
                    if (usage > data.resource_capacity(r)) {
                        feasible = false;
                        break;
                    }
                }
                if (!feasible) {
                    continue;
                }

                int delta = candidate.duration - current_dur;
                if (delta < best_delta) {
                    best_delta = delta;
                    best_op = op;
                    best_mode = m;
                }
            }
        }

        if (best_op >= 0 && best_delta < 0) {
            assignment.set_mode(best_op, best_mode);
            improved = true;
        }
    }
}

}  // namespace coso
