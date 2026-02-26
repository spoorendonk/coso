#include <iostream>

#include "model/packing_model.h"

int main()
{
    coso::PackingModel model;
    model.add_bin_type({.capacity = {10}, .cost = 1, .count = 0});
    model.add_item({.size = {6}});
    model.add_item({.size = {4}});
    model.add_item({.size = {3}});
    model.minimize_bins();

    coso::Result result = model.solve(coso::TimeLimit(1.0, 0.05));
    std::cout << "packing feasible=" << result.feasible()
              << " bins=" << result.num_bins()
              << " cost=" << result.cost() << "\n";
    return result.feasible() ? 0 : 1;
}
