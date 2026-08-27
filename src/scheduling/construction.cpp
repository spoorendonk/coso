#include "scheduling/construction.h"

#include <algorithm>
#include <climits>
#include <numeric>
#include <queue>
#include <vector>

namespace coso {

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

namespace {

/// Build successor lists and in-degree counts from precedence arcs.
struct PrecedenceInfo {
    std::vector<std::vector<int>> successors;  // successors[op] = list of ops
    std::vector<int> in_degree;                // number of unsatisfied preds
};

PrecedenceInfo build_precedence(ScheduleData const& data) {
    int n = data.num_operations();
    PrecedenceInfo info;
    info.successors.resize(n);
    info.in_degree.assign(n, 0);

    for (auto const& arc : data.precedences()) {
        info.successors[arc.before].push_back(arc.after);
        info.in_degree[arc.after]++;
    }
    return info;
}

/// Choose the best machine for an operation (shortest processing time).
/// Returns (machine, duration).
std::pair<int, int> best_machine(ScheduleData const& data, int op) {
    int best_m = -1;
    int best_d = INT_MAX;
    for (int m = 0; m < data.num_machines(); ++m) {
        int pt = data.processing_time(op, m);
        if (pt < best_d) {
            best_d = pt;
            best_m = m;
        }
    }
    return {best_m, best_d};
}

/// Compute makespan from a schedule vector.
int compute_makespan(ScheduleData const& data, std::vector<Result::OpSchedule> const& sched) {
    int ms = 0;
    for (int o = 0; o < data.num_operations(); ++o) {
        int m = sched[o].machine;
        int pt = data.processing_time(o, m);
        int finish = sched[o].start_time + pt;
        ms = std::max(ms, finish);
    }
    return ms;
}

/// Check whether scheduling operation op at start_time on machine m
/// would violate any renewable resource capacity constraint at any
/// time unit during its execution.
///
/// resource_profile[r][t] = current usage of resource r at time t.
bool resource_feasible(ScheduleData const& data, int op, int start_time, int duration,
                       std::vector<std::vector<int>> const& resource_profile) {
    if (data.num_resources() == 0) {
        return true;
    }

    for (int r = 0; r < data.num_resources(); ++r) {
        int usage = data.resource_usage(op, r);
        if (usage == 0) {
            continue;
        }
        int cap = data.resource_capacity(r);
        for (int t = start_time; t < start_time + duration; ++t) {
            int current =
                (t < static_cast<int>(resource_profile[r].size())) ? resource_profile[r][t] : 0;
            if (current + usage > cap) {
                return false;
            }
        }
    }
    return true;
}

/// Update resource profile after scheduling an operation.
void update_resource_profile(ScheduleData const& data, int op, int start_time, int duration,
                             std::vector<std::vector<int>>& resource_profile) {
    for (int r = 0; r < data.num_resources(); ++r) {
        int usage = data.resource_usage(op, r);
        if (usage == 0) {
            continue;
        }
        int end = start_time + duration;
        if (end > static_cast<int>(resource_profile[r].size())) {
            resource_profile[r].resize(end, 0);
        }
        for (int t = start_time; t < end; ++t) {
            resource_profile[r][t] += usage;
        }
    }
}

}  // namespace

// ---------------------------------------------------------------------------
//  SGS — Serial Generation Scheme
// ---------------------------------------------------------------------------

Result construct_sgs(ScheduleData const& data) {
    int n = data.num_operations();
    auto [successors, in_degree] = build_precedence(data);

    // Machine availability: next free time for each machine.
    std::vector<int> machine_free(data.num_machines(), 0);

    // Completion time per operation (-1 = not yet scheduled).
    std::vector<int> completion(n, -1);

    // Resource profiles: resource_profile[r][t] = current usage at time t.
    std::vector<std::vector<int>> resource_profile(data.num_resources());

    Result result;
    result.schedule_.resize(n);

    int scheduled = 0;
    while (scheduled < n) {
        // Collect all ready operations (in-degree = 0, not yet scheduled).
        int best_op = -1;
        int best_start = INT_MAX;
        int best_mach = -1;
        int best_dur = 0;

        for (int op = 0; op < n; ++op) {
            if (completion[op] >= 0) {
                continue;  // already scheduled
            }
            if (in_degree[op] > 0) {
                continue;  // predecessors not done
            }

            // Earliest start from predecessors.
            int es = 0;
            // (predecessors are all completed since in_degree == 0,
            //  but we still need to check the actual completion times
            //  from the precedence arcs.)
            // We already know all predecessors are scheduled, so es is
            // max of their completion times. We can get this from the arcs,
            // but it's simpler to iterate over all arcs targeting op.
            // Actually, since we decremented in_degree, we can just check
            // the completion times directly via the original arcs.
            for (auto const& arc : data.precedences()) {
                if (arc.after == op) {
                    es = std::max(es, completion[arc.before]);
                }
            }

            // Also respect job release time.
            auto const& opdata = data.operation(op);
            if (opdata.job >= 0) {
                es = std::max(es, data.job(opdata.job).release_time);
            }

            // Pick best machine for this operation.
            auto [m, dur] = best_machine(data, op);
            if (m < 0) {
                continue;  // no valid machine
            }

            // Earliest start on this machine.
            int start = std::max(es, machine_free[m]);

            // Resource feasibility: advance start until feasible.
            while (!resource_feasible(data, op, start, dur, resource_profile)) {
                ++start;
            }

            // Pick operation with smallest earliest start (ties: lowest index).
            if (start < best_start || (start == best_start && op < best_op)) {
                best_op = op;
                best_start = start;
                best_mach = m;
                best_dur = dur;
            }
        }

        if (best_op < 0) {
            break;  // should not happen for valid instances
        }

        // Schedule best_op.
        result.schedule_[best_op] = {.machine = best_mach, .start_time = best_start};
        completion[best_op] = best_start + best_dur;
        machine_free[best_mach] = best_start + best_dur;

        update_resource_profile(data, best_op, best_start, best_dur, resource_profile);

        // Decrement in-degree for successors.
        for (int succ : successors[best_op]) {
            --in_degree[succ];
        }

        ++scheduled;
    }

    result.makespan_ = compute_makespan(data, result.schedule_);
    result.cost_ = result.makespan_;
    result.feasible_ = (scheduled == n);
    return result;
}

// ---------------------------------------------------------------------------
//  NEH heuristic
// ---------------------------------------------------------------------------

Result construct_neh(ScheduleData const& data) {
    int nj = data.num_jobs();
    int n = data.num_operations();

    // Compute total processing time per job (using best machine for each op).
    std::vector<int> job_total(nj, 0);
    for (int j = 0; j < nj; ++j) {
        for (int op : data.job(j).operations) {
            auto [m, d] = best_machine(data, op);
            job_total[j] += d;
        }
    }

    // Sort jobs by descending total processing time.
    std::vector<int> job_order(nj);
    std::iota(job_order.begin(), job_order.end(), 0);
    std::sort(job_order.begin(), job_order.end(),
              [&](int a, int b) { return job_total[a] > job_total[b]; });

    // We maintain the sequence of jobs inserted so far.
    // For each new job we try all insertion positions and keep the best.
    std::vector<int> sequence;
    sequence.reserve(nj);

    // Lambda: given a job sequence, build a schedule and return makespan.
    // We use a simple left-to-right scheduler respecting intra-job precedence.
    auto evaluate =
        [&](std::vector<int> const& seq) -> std::pair<std::vector<Result::OpSchedule>, int> {
        std::vector<Result::OpSchedule> sched(n);
        std::vector<int> machine_free(data.num_machines(), 0);
        std::vector<int> op_completion(n, 0);

        for (int j : seq) {
            auto const& ops = data.job(j).operations;
            int prev_finish = data.job(j).release_time;
            for (int op : ops) {
                auto [m, dur] = best_machine(data, op);
                int start = std::max(prev_finish, machine_free[m]);
                sched[op] = {.machine = m, .start_time = start};
                op_completion[op] = start + dur;
                machine_free[m] = start + dur;
                prev_finish = start + dur;
            }
        }

        int ms = 0;
        for (int o = 0; o < n; ++o) {
            int m = sched[o].machine;
            int pt = data.processing_time(o, m);
            ms = std::max(ms, sched[o].start_time + pt);
        }
        return {sched, ms};
    };

    for (int ji = 0; ji < nj; ++ji) {
        int job = job_order[ji];
        int best_pos = 0;
        int best_ms = INT_MAX;

        // Try inserting job at each position 0..sequence.size().
        for (int pos = 0; pos <= static_cast<int>(sequence.size()); ++pos) {
            std::vector<int> trial = sequence;
            trial.insert(trial.begin() + pos, job);
            auto [sched, ms] = evaluate(trial);
            if (ms < best_ms) {
                best_ms = ms;
                best_pos = pos;
            }
        }

        sequence.insert(sequence.begin() + best_pos, job);
    }

    auto [final_sched, final_ms] = evaluate(sequence);
    Result result;
    result.schedule_ = std::move(final_sched);
    result.makespan_ = final_ms;
    result.cost_ = final_ms;
    result.feasible_ = true;
    return result;
}

// ---------------------------------------------------------------------------
//  Dispatching rule construction
// ---------------------------------------------------------------------------

Result construct_dispatch(ScheduleData const& data, DispatchRule rule) {
    int n = data.num_operations();
    auto [successors, in_degree] = build_precedence(data);

    std::vector<int> machine_free(data.num_machines(), 0);
    std::vector<int> completion(n, -1);

    Result result;
    result.schedule_.resize(n);

    // Priority comparator: SPT or LPT based on the processing duration.
    // We compute duration using best_machine.
    std::vector<int> op_duration(n);
    for (int o = 0; o < n; ++o) {
        auto [m, d] = best_machine(data, o);
        op_duration[o] = d;
    }

    // Ready list: operations with in_degree == 0.
    std::vector<int> ready;
    for (int o = 0; o < n; ++o) {
        if (in_degree[o] == 0) {
            ready.push_back(o);
        }
    }

    // Sort ready list by priority rule.
    auto sort_ready = [&]() {
        std::sort(ready.begin(), ready.end(), [&](int a, int b) {
            if (rule == DispatchRule::SPT) {
                if (op_duration[a] != op_duration[b]) {
                    return op_duration[a] < op_duration[b];
                }
            } else {
                if (op_duration[a] != op_duration[b]) {
                    return op_duration[a] > op_duration[b];
                }
            }
            return a < b;  // tie-break by index
        });
    };

    int scheduled = 0;
    while (!ready.empty()) {
        sort_ready();

        // Schedule the highest-priority ready operation.
        int op = ready.front();
        ready.erase(ready.begin());

        auto [m, dur] = best_machine(data, op);

        // Earliest start from predecessors.
        int es = 0;
        for (auto const& arc : data.precedences()) {
            if (arc.after == op && completion[arc.before] >= 0) {
                es = std::max(es, completion[arc.before]);
            }
        }

        auto const& opdata = data.operation(op);
        if (opdata.job >= 0) {
            es = std::max(es, data.job(opdata.job).release_time);
        }

        int start = std::max(es, machine_free[m]);
        result.schedule_[op] = {.machine = m, .start_time = start};
        completion[op] = start + dur;
        machine_free[m] = start + dur;

        // Update successors.
        for (int succ : successors[op]) {
            --in_degree[succ];
            if (in_degree[succ] == 0) {
                ready.push_back(succ);
            }
        }

        ++scheduled;
    }

    result.makespan_ = compute_makespan(data, result.schedule_);
    result.cost_ = result.makespan_;
    result.feasible_ = (scheduled == n);
    return result;
}

}  // namespace coso
