#include "model/assignment_model.h"

namespace coso {

int AssignmentModel::add_shift_type(ShiftTypeParams /*p*/)
{
    return 0;
}

int AssignmentModel::add_employee(EmployeeParams /*p*/)
{
    return 0;
}

void AssignmentModel::set_horizon(int /*days*/) {}

void AssignmentModel::add_demand(int /*shift_type*/, int /*day*/, DemandParams /*p*/) {}

void AssignmentModel::add_demand(int /*shift_type*/, DemandParams /*p*/) {}

void AssignmentModel::set_max_consecutive_shifts(int /*n*/) {}

void AssignmentModel::set_min_rest_between_shifts(int /*hours*/) {}

void AssignmentModel::add_forbidden_sequence(
    const std::vector<int>& /*shift_types*/) {}

void AssignmentModel::add_preference(
    int /*employee*/, int /*day*/, int /*shift_type*/, int /*weight*/) {}

void AssignmentModel::add_unavailability(int /*employee*/, int /*day*/) {}

void AssignmentModel::set_published_schedule(
    const std::vector<std::vector<int>>& /*schedule*/) {}

void AssignmentModel::set_change_penalty(int /*penalty*/) {}

Result AssignmentModel::solve(TimeLimit /*tl*/)
{
    return {};
}

} // namespace coso
