#include "scheduling/schedule_data.h"

#include <climits>
#include <stdexcept>

namespace coso {

// ---------------------------------------------------------------------------
//  Builder methods
// ---------------------------------------------------------------------------

int ScheduleData::Builder::add_machine(MachineParams p) {
    int idx = static_cast<int>(machines_.size());
    machines_.push_back(std::move(p));
    return idx;
}

int ScheduleData::Builder::add_job(JobParams p) {
    int idx = static_cast<int>(jobs_.size());
    jobs_.push_back(std::move(p));
    job_operations_.emplace_back();  // empty operation list for this job
    return idx;
}

int ScheduleData::Builder::add_operation(int job, OperationParams p) {
    if (job < 0 || job >= static_cast<int>(jobs_.size())) {
        throw std::out_of_range("ScheduleData::Builder::add_operation: invalid job index");
    }

    int idx = static_cast<int>(operations_.size());
    operations_.push_back({.job = job, .params = std::move(p)});
    job_operations_[job].push_back(idx);
    return idx;
}

void ScheduleData::Builder::set_setup_time(int from_op, int to_op, int machine, int time) {
    setup_entries_.push_back({from_op, to_op, machine, time});
}

void ScheduleData::Builder::set_setup_time(int from_op, int to_op, int time) {
    setup_uniform_entries_.push_back({from_op, to_op, time});
}

void ScheduleData::Builder::add_machine_available(int machine, int start, int end) {
    calendar_entries_.push_back({machine, start, end});
}

void ScheduleData::Builder::set_objective(ScheduleObjective obj) {
    objective_ = obj;
}

// ---------------------------------------------------------------------------
//  build()
// ---------------------------------------------------------------------------

ScheduleData ScheduleData::Builder::build() const {
    ScheduleData data;

    int num_machines = static_cast<int>(machines_.size());
    int num_jobs = static_cast<int>(jobs_.size());
    int num_operations = static_cast<int>(operations_.size());

    data.num_machines_ = num_machines;
    data.num_jobs_ = num_jobs;
    data.num_operations_ = num_operations;
    data.objective_ = objective_;

    // Jobs.
    data.jobs_.resize(num_jobs);
    for (int j = 0; j < num_jobs; ++j) {
        auto& jd = data.jobs_[j];
        jd.due_date = jobs_[j].due_date;
        jd.weight = jobs_[j].weight;
        jd.operations = job_operations_[j];
    }

    // Operations.
    data.operations_.resize(num_operations);
    for (int o = 0; o < num_operations; ++o) {
        auto const& src = operations_[o];
        auto& dst = data.operations_[o];
        dst.job = src.job;
        dst.fixed_machine = src.params.machine;
        dst.duration = src.params.duration;
        dst.eligible_machines = src.params.eligible_machines;
        dst.durations_per_machine = src.params.durations_per_machine;
    }

    // Processing time matrix (operation x machine).
    // INT_MAX means the operation cannot run on that machine.
    data.processing_times_.assign(num_operations * num_machines, INT_MAX);
    for (int o = 0; o < num_operations; ++o) {
        auto const& op = data.operations_[o];
        if (op.fixed_machine >= 0 && op.fixed_machine < num_machines) {
            // Fixed machine assignment.
            data.processing_times_[o * num_machines + op.fixed_machine] = op.duration;
        } else if (!op.eligible_machines.empty()) {
            // FJSP: eligible machines with per-machine durations.
            for (int i = 0; i < static_cast<int>(op.eligible_machines.size()); ++i) {
                int m = op.eligible_machines[i];
                if (m >= 0 && m < num_machines) {
                    int dur = (i < static_cast<int>(op.durations_per_machine.size()))
                                  ? op.durations_per_machine[i]
                                  : op.duration;
                    data.processing_times_[o * num_machines + m] = dur;
                }
            }
        } else if (num_machines > 0) {
            // No machine specified — assume operation can run on any machine
            // with its fixed duration.
            for (int m = 0; m < num_machines; ++m) {
                data.processing_times_[o * num_machines + m] = op.duration;
            }
        }
    }

    // Precedence arcs: intra-job (consecutive operations).
    for (int j = 0; j < num_jobs; ++j) {
        auto const& ops = job_operations_[j];
        for (int i = 0; i + 1 < static_cast<int>(ops.size()); ++i) {
            data.precedences_.push_back({ops[i], ops[i + 1]});
        }
    }

    // Setup times (if any were specified).
    if (!setup_entries_.empty() || !setup_uniform_entries_.empty()) {
        SetupTimeMatrix stm(num_operations, num_machines);
        for (auto const& e : setup_uniform_entries_) {
            stm.set(e.from, e.to, e.time);
        }
        for (auto const& e : setup_entries_) {
            stm.set(e.from, e.to, e.machine, e.time);
        }
        data.setup_times_ = std::move(stm);
    }

    // Machine calendars (if any were specified).
    if (!calendar_entries_.empty()) {
        MachineCalendar cal(num_machines);
        for (auto const& e : calendar_entries_) {
            cal.add_available(e.machine, e.start, e.end);
        }
        data.calendar_ = std::move(cal);
    }

    return data;
}

}  // namespace coso
