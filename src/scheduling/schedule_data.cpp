#include "scheduling/schedule_data.h"

#include <climits>
#include <stdexcept>
#include <string>

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

    // Validate machine references before anything reads them (#234). A v1 shop
    // operation occupies exactly one machine (#202), so it names either one
    // fixed machine or a non-empty set of eligible ones, all in range.
    // Machine-free operations are an RCPSP extension, not yet declarable.
    for (int o = 0; o < num_operations; ++o) {
        auto const& p = operations_[o].params;
        auto where = [&] { return "ScheduleData::Builder::build: operation " + std::to_string(o); };
        auto in_range = [&](int m) { return m >= 0 && m < num_machines; };
        auto range = [&] { return " (machines are 0.." + std::to_string(num_machines - 1) + ")"; };
        bool const fixed = p.machine != -1;
        if (fixed && !p.eligible_machines.empty()) {
            throw std::invalid_argument(where() + " declares both machine and eligible_machines");
        }
        if (!fixed && p.eligible_machines.empty()) {
            throw std::invalid_argument(where() + " declares no machine");
        }
        if (fixed && !in_range(p.machine)) {
            throw std::invalid_argument(where() + ": machine " + std::to_string(p.machine) +
                                        " does not exist" + range());
        }
        for (int m : p.eligible_machines) {
            if (!in_range(m)) {
                throw std::invalid_argument(where() + ": eligible machine " + std::to_string(m) +
                                            " does not exist" + range());
            }
        }
        if (!fixed && p.durations_per_machine.size() != p.eligible_machines.size()) {
            throw std::invalid_argument(
                where() + ": durations_per_machine has " +
                std::to_string(p.durations_per_machine.size()) + " entries for " +
                std::to_string(p.eligible_machines.size()) + " eligible machines");
        }
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
        if (op.fixed_machine >= 0) {
            data.processing_times_[o * num_machines + op.fixed_machine] = op.duration;
        } else {
            for (int i = 0; i < static_cast<int>(op.eligible_machines.size()); ++i) {
                data.processing_times_[o * num_machines + op.eligible_machines[i]] =
                    op.durations_per_machine[i];
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
