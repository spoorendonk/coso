#include "model/rostering_model.h"

#include <iostream>

int main() {
    coso::RosteringModel model;
    int day_shift = model.add_shift_type({.name = "Day", .duration_hours = 8});
    model.add_employee({.name = "Alice"});
    model.add_employee({.name = "Bob"});
    model.set_horizon(3);
    for (int day = 0; day < 3; ++day) {
        model.add_demand(day_shift, day, {.min_employees = 1, .max_employees = 1});
    }

    coso::Result result = model.solve(coso::TimeLimit(1.0, 0.05));
    std::cout << "rostering feasible=" << result.feasible() << " days=" << result.roster().size()
              << " unassigned=" << result.unassigned().size() << "\n";
    return result.feasible() ? 0 : 1;
}
