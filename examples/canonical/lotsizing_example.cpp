#include <iostream>

#include "model/lotsizing_model.h"

int main()
{
    coso::LotSizingModel model;
    model.set_num_periods(3);
    int p = model.add_product(10.0, 1.0, 2.0, 0.5);
    model.set_demand(p, 0, 3.0);
    model.set_demand(p, 1, 4.0);
    model.set_demand(p, 2, 2.0);
    model.set_capacity(0, 10.0);
    model.set_capacity(1, 10.0);
    model.set_capacity(2, 10.0);

    coso::Result result = model.solve(coso::TimeLimit(1.0, 0.05));
    std::cout << "lotsizing feasible=" << result.feasible()
              << " cost=" << result.cost()
              << " products=" << result.production().size() << "\n";
    return result.feasible() ? 0 : 1;
}
