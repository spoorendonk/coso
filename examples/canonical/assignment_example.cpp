#include <iostream>

#include "model/assignment_model.h"

int main()
{
    coso::AssignmentModel model;
    int day_shift = model.add_shift_type({.name = "Day", .start_hour = 8, .end_hour = 16});
    model.add_employee({.name = "Alice"});
    model.add_employee({.name = "Bob"});
    model.set_horizon(3);
    model.add_demand(day_shift, {.min_employees = 1, .max_employees = 1});

    coso::Result result = model.solve(coso::TimeLimit(1.0, 0.05));
    std::cout << "assignment feasible=" << result.feasible()
              << " days=" << result.assignments().size()
              << " unassigned=" << result.unassigned().size() << "\n";
    return result.feasible() ? 0 : 1;
}
