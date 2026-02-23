#include "model/routing_model.h"

namespace coso {

int RoutingModel::add_depot(double /*x*/, double /*y*/, DepotParams /*p*/)
{
    return 0;
}

int RoutingModel::add_depot(int /*id*/, DepotParams /*p*/)
{
    return 0;
}

int RoutingModel::add_vehicle_type(int /*count*/, VehicleTypeParams /*p*/)
{
    return 0;
}

int RoutingModel::add_client(double /*x*/, double /*y*/, ClientParams /*p*/)
{
    return 0;
}

int RoutingModel::add_client(int /*id*/, ClientParams /*p*/)
{
    return 0;
}

int RoutingModel::add_pickup(double /*x*/, double /*y*/, ClientParams /*p*/)
{
    return 0;
}

int RoutingModel::add_delivery(double /*x*/, double /*y*/, ClientParams /*p*/)
{
    return 0;
}

void RoutingModel::add_request(int /*pickup*/, int /*delivery*/) {}

void RoutingModel::add_pickup_delivery(int /*pickup*/, int /*delivery*/) {}

int RoutingModel::add_client_group()
{
    return 0;
}

void RoutingModel::set_distance(int /*from*/, int /*to*/, int /*dist*/) {}

void RoutingModel::set_duration(int /*from*/, int /*to*/, int /*dur*/) {}

void RoutingModel::set_profile(int /*profile*/) {}

void RoutingModel::set_profile_distance(
    int /*profile*/, int /*from*/, int /*to*/, int /*dist*/) {}

void RoutingModel::set_profile_duration(
    int /*profile*/, int /*from*/, int /*to*/, int /*dur*/) {}

void RoutingModel::set_cost_matrix(
    int /*profile*/, int /*from*/, int /*to*/, int /*cost*/) {}

void RoutingModel::set_initial_routes(
    const std::vector<std::vector<int>>& /*routes*/) {}

void RoutingModel::pin(int /*client_id*/) {}

Result RoutingModel::solve(TimeLimit /*tl*/)
{
    return {};
}

Result solve(const std::string& /*instance_path*/, TimeLimit /*tl*/)
{
    return {};
}

} // namespace coso
