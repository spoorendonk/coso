#include <iostream>

#include "model/routing_model.h"

int main()
{
    coso::RoutingModel model;
    model.add_depot(0.0, 0.0);
    model.add_vehicle_type(1, {.capacity = {10}});
    model.add_client(1.0, 0.0, {.demand = {3}});
    model.add_client(0.0, 1.0, {.demand = {4}});

    coso::Result result = model.solve(coso::TimeLimit(1.0, 0.05));
    std::cout << "routing feasible=" << result.feasible()
              << " cost=" << result.cost()
              << " routes=" << result.routes().size() << "\n";
    return result.feasible() ? 0 : 1;
}
