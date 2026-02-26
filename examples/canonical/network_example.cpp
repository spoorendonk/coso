#include <iostream>

#include "model/network_model.h"

int main()
{
    coso::NetworkModel model;
    int source = model.add_node(5, "source");
    int sink = model.add_node(-5, "sink");
    model.add_arc(source, sink, 2, 0, 5);

    coso::Result result = model.solve(coso::TimeLimit(1.0, 0.05));
    std::cout << "network feasible=" << result.feasible()
              << " cost=" << result.cost()
              << " commodities=" << result.flows().size() << "\n";
    return result.feasible() ? 0 : 1;
}
