#include "scheduling/schedule_solution.h"

#include <algorithm>
#include <cassert>
#include <climits>

namespace coso {

ScheduleSolution::ScheduleSolution(ScheduleData const& data)
    : data_(data), assignments_(data.num_operations()) {}

// -------------------------------------------------------------------------- //
//  Mutation                                                                    //
// -------------------------------------------------------------------------- //

void ScheduleSolution::assign(int op, int machine, int start_time) {
    assert(op >= 0 && op < data_.num_operations());
    assert(machine >= 0 && machine < data_.num_machines());
    assert(start_time >= 0);

    if (!assignments_[op].assigned()) {
        ++num_assigned_;
    }

    assignments_[op].machine = machine;
    assignments_[op].start_time = start_time;
}

void ScheduleSolution::unassign(int op) {
    assert(op >= 0 && op < data_.num_operations());

    if (assignments_[op].assigned()) {
        --num_assigned_;
    }

    assignments_[op].machine = -1;
    assignments_[op].start_time = -1;
}

// -------------------------------------------------------------------------- //
//  Objective values                                                            //
// -------------------------------------------------------------------------- //

int ScheduleSolution::completion_time(int op) const {
    assert(op >= 0 && op < data_.num_operations());
    auto const& a = assignments_[op];
    if (!a.assigned()) {
        return -1;
    }
    int pt = data_.processing_time(op, a.machine);
    return a.start_time + pt;
}

int ScheduleSolution::job_completion_time(int job) const {
    assert(job >= 0 && job < data_.num_jobs());
    auto const& ops = data_.job(job).operations;
    if (ops.empty()) {
        return 0;
    }

    // Completion time of the last operation in the job.
    int last_op = ops.back();
    return completion_time(last_op);
}

int ScheduleSolution::makespan() const {
    int ms = 0;
    for (int o = 0; o < data_.num_operations(); ++o) {
        int ct = completion_time(o);
        if (ct > ms) {
            ms = ct;
        }
    }
    return ms;
}

int ScheduleSolution::total_weighted_tardiness() const {
    int twt = 0;
    for (int j = 0; j < data_.num_jobs(); ++j) {
        int cj = job_completion_time(j);
        if (cj < 0) {
            continue;  // job has unassigned operations
        }
        int due = data_.job(j).due_date;
        int tardiness = std::max(0, cj - due);
        twt += data_.job(j).weight * tardiness;
    }
    return twt;
}

int ScheduleSolution::total_flow_time() const {
    int tft = 0;
    for (int j = 0; j < data_.num_jobs(); ++j) {
        int cj = job_completion_time(j);
        if (cj < 0) {
            continue;
        }
        tft += cj;
    }
    return tft;
}

int ScheduleSolution::objective() const {
    switch (data_.objective()) {
        case ScheduleObjective::Makespan:
            return makespan();
        case ScheduleObjective::TotalWeightedTardiness:
            return total_weighted_tardiness();
        case ScheduleObjective::TotalFlowTime:
            return total_flow_time();
    }
    return makespan();  // unreachable, but silence warnings
}

// -------------------------------------------------------------------------- //
//  Feasibility                                                                 //
// -------------------------------------------------------------------------- //

bool ScheduleSolution::all_assigned() const {
    for (int o = 0; o < data_.num_operations(); ++o) {
        auto const& od = data_.operation(o);
        if (od.optional) {
            continue;
        }
        if (!assignments_[o].assigned()) {
            return false;
        }
    }
    return true;
}

bool ScheduleSolution::no_machine_overlaps() const {
    for (int m = 0; m < data_.num_machines(); ++m) {
        auto ops = machine_operations(m);
        // ops are sorted by start time
        for (int i = 0; i + 1 < static_cast<int>(ops.size()); ++i) {
            int ct_i = completion_time(ops[i]);
            int st_next = assignments_[ops[i + 1]].start_time;
            if (ct_i > st_next) {
                return false;
            }
        }
    }
    return true;
}

bool ScheduleSolution::precedences_respected() const {
    for (auto const& prec : data_.precedences()) {
        auto const& a_before = assignments_[prec.before];
        auto const& a_after = assignments_[prec.after];

        // If either is unassigned, skip (all_assigned checks that separately).
        if (!a_before.assigned() || !a_after.assigned()) {
            continue;
        }

        int ct_before = completion_time(prec.before);
        if (ct_before > a_after.start_time) {
            return false;
        }
    }
    return true;
}

bool ScheduleSolution::feasible() const {
    if (!all_assigned()) {
        return false;
    }

    // Check machine eligibility for each assigned operation.
    for (int o = 0; o < data_.num_operations(); ++o) {
        auto const& a = assignments_[o];
        if (!a.assigned()) {
            continue;
        }
        // processing_time returns INT_MAX if the op can't run on that machine.
        if (data_.processing_time(o, a.machine) == INT_MAX) {
            return false;
        }
    }

    if (!no_machine_overlaps()) {
        return false;
    }

    if (!precedences_respected()) {
        return false;
    }

    return true;
}

// -------------------------------------------------------------------------- //
//  Accessors                                                                   //
// -------------------------------------------------------------------------- //

std::vector<int> ScheduleSolution::machine_operations(int machine) const {
    assert(machine >= 0 && machine < data_.num_machines());

    std::vector<int> ops;
    for (int o = 0; o < data_.num_operations(); ++o) {
        auto const& a = assignments_[o];
        if (a.assigned() && a.machine == machine) {
            ops.push_back(o);
        }
    }

    std::sort(ops.begin(), ops.end(), [this](int a, int b) {
        return assignments_[a].start_time < assignments_[b].start_time;
    });

    return ops;
}

}  // namespace coso
