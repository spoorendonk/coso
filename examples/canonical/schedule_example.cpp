#include "model/schedule_model.h"

#include <iostream>

int main() {
    coso::ScheduleModel model;
    model.add_machine({});
    int job = model.add_job({});
    model.add_operation(job, {.machine = 0, .duration = 3});
    model.minimize_makespan();

    coso::Result result = model.solve(coso::TimeLimit(1.0, 0.05));
    std::cout << "schedule feasible=" << result.feasible() << " makespan=" << result.makespan()
              << " ops=" << result.schedule().size() << "\n";
    return result.feasible() ? 0 : 1;
}
