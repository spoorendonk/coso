#include "model/schedule_model.h"

namespace coso {

int ScheduleModel::add_machine(MachineParams /*p*/)
{
    return 0;
}

int ScheduleModel::add_job(JobParams /*p*/)
{
    return 0;
}

int ScheduleModel::add_operation(int /*job*/, OperationParams /*p*/)
{
    return 0;
}

int ScheduleModel::add_resource(int /*capacity*/)
{
    return 0;
}

void ScheduleModel::set_resource_usage(
    int /*operation*/, int /*resource*/, int /*amount*/) {}

void ScheduleModel::add_precedence(int /*op_before*/, int /*op_after*/) {}

void ScheduleModel::set_objective(ScheduleObjective /*obj*/) {}

void ScheduleModel::minimize_makespan() {}

void ScheduleModel::set_initial_schedule(
    const std::vector<std::pair<int, int>>& /*op_assignments*/) {}

Result ScheduleModel::solve(TimeLimit /*tl*/)
{
    return {};
}

Result solve_jsp(const std::string& /*instance_path*/, TimeLimit /*tl*/)
{
    return {};
}

} // namespace coso
