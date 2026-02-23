#include "model/routing_model.h"
#include "model/instance_reader.h"
#include "routing/cost_evaluator.h"
#include "routing/problem_data.h"
#include "routing/solution.h"
#include "search/iterated_local_search.h"
#include "search/stop_criterion.h"

#include <chrono>
#include <cmath>
#include <stdexcept>

namespace coso {

// ---------------------------------------------------------------------------
//  Depot / Client / VehicleType registration
// ---------------------------------------------------------------------------

int RoutingModel::add_depot(double x, double y, DepotParams p)
{
    int idx = static_cast<int>(depots_.size());
    depots_.push_back({.x = x, .y = y, .has_coord = true,
                       .explicit_id = -1, .params = p});
    return idx;
}

int RoutingModel::add_depot(int id, DepotParams p)
{
    int idx = static_cast<int>(depots_.size());
    depots_.push_back({.x = 0.0, .y = 0.0, .has_coord = false,
                       .explicit_id = id, .params = p});
    return idx;
}

int RoutingModel::add_vehicle_type(int count, VehicleTypeParams p)
{
    int idx = static_cast<int>(vehicle_types_.size());
    vehicle_types_.push_back({.count = count, .params = std::move(p)});
    return idx;
}

int RoutingModel::add_client(double x, double y, ClientParams p)
{
    int idx = static_cast<int>(clients_.size());
    clients_.push_back({.x = x, .y = y, .has_coord = true,
                        .explicit_id = -1, .params = std::move(p)});
    return idx;
}

int RoutingModel::add_client(int id, ClientParams p)
{
    int idx = static_cast<int>(clients_.size());
    clients_.push_back({.x = 0.0, .y = 0.0, .has_coord = false,
                        .explicit_id = id, .params = std::move(p)});
    return idx;
}

int RoutingModel::add_pickup(double x, double y, ClientParams p)
{
    return add_client(x, y, std::move(p));
}

int RoutingModel::add_delivery(double x, double y, ClientParams p)
{
    return add_client(x, y, std::move(p));
}

void RoutingModel::add_request(int pickup, int delivery)
{
    requests_.emplace_back(pickup, delivery);
}

void RoutingModel::add_pickup_delivery(int pickup, int delivery)
{
    add_request(pickup, delivery);
}

int RoutingModel::add_client_group()
{
    return next_group_id_++;
}

// ---------------------------------------------------------------------------
//  Distance / duration / cost matrix setters
// ---------------------------------------------------------------------------

void RoutingModel::set_distance(int from, int to, int dist)
{
    dist_entries_.push_back({current_profile_, from, to, dist});
}

void RoutingModel::set_duration(int from, int to, int dur)
{
    dur_entries_.push_back({current_profile_, from, to, dur});
}

void RoutingModel::set_profile(int profile)
{
    current_profile_ = profile;
}

void RoutingModel::set_profile_distance(int profile, int from, int to, int dist)
{
    dist_entries_.push_back({profile, from, to, dist});
}

void RoutingModel::set_profile_duration(int profile, int from, int to, int dur)
{
    dur_entries_.push_back({profile, from, to, dur});
}

void RoutingModel::set_cost_matrix(int profile, int from, int to, int cost)
{
    cost_entries_.push_back({profile, from, to, cost});
}

// ---------------------------------------------------------------------------
//  Warm start / pin
// ---------------------------------------------------------------------------

void RoutingModel::set_initial_routes(const std::vector<std::vector<int>>& routes)
{
    initial_routes_ = routes;
}

void RoutingModel::pin(int client_id)
{
    pinned_.push_back(client_id);
}

// ---------------------------------------------------------------------------
//  solve()
// ---------------------------------------------------------------------------

Result RoutingModel::solve(TimeLimit tl)
{
    auto wall_start = std::chrono::steady_clock::now();

    // Validate: need at least one depot and one vehicle type.
    if (depots_.empty() || vehicle_types_.empty()) {
        return {};  // cannot solve without depot/vehicles
    }

    // -----------------------------------------------------------------------
    //  Build ProblemData
    // -----------------------------------------------------------------------

    ProblemData::Builder builder;

    // Add depots.
    for (auto const& d : depots_) {
        Coord coord{d.x, d.y};
        builder.add_depot(coord, d.params);
    }

    // Add clients.
    for (auto const& c : clients_) {
        Coord coord{c.x, c.y};
        builder.add_client(coord, c.params);
    }

    // Add vehicle types.
    for (auto const& vt : vehicle_types_) {
        builder.add_vehicle_type(vt.count, vt.params);
    }

    // Add pickup-delivery requests.
    for (auto const& [pickup, delivery] : requests_) {
        builder.add_request(pickup, delivery);
    }

    // Total number of nodes (depots + clients) for matrix indexing.
    int num_depots  = static_cast<int>(depots_.size());
    int num_clients = static_cast<int>(clients_.size());
    int n = num_depots + num_clients;

    // Add explicit distance entries.
    // The user-facing API uses a node numbering where depot ids and client ids
    // are separate (depot 0..D-1, client 0..C-1). The ProblemData Builder uses
    // full node numbering (depot 0..D-1, client D..D+C-1). Distance entries
    // from the user use full node numbering (from set_distance(from, to, dist)
    // where from/to are node indices in the model).
    //
    // For the RoutingModel API, set_distance(from, to, dist) uses node indices
    // where depots are 0..D-1 and clients are D..D+C-1 (matching ProblemData).
    for (auto const& e : dist_entries_) {
        builder.set_distance(e.profile, e.from, e.to, e.value);
    }
    for (auto const& e : dur_entries_) {
        builder.set_duration(e.profile, e.from, e.to, e.value);
    }
    for (auto const& e : cost_entries_) {
        builder.set_cost(e.profile, e.from, e.to, e.value);
    }

    // Build with default granularity.
    int granular_k = std::min(40, std::max(0, num_clients - 1));
    ProblemData data = builder.build(granular_k);

    // -----------------------------------------------------------------------
    //  Run ILS
    // -----------------------------------------------------------------------

    CostEvaluator eval;  // default penalty weights
    StopCriterion stop(tl.seconds);

    IteratedLocalSearch ils(data);
    Solution best = ils.run(eval, stop);

    // -----------------------------------------------------------------------
    //  Convert Solution to Result
    // -----------------------------------------------------------------------

    Result result;
    result.feasible_ = best.feasible();
    result.cost_ = static_cast<double>(best.total_distance());

    // Extract non-empty routes.
    for (int v = 0; v < best.num_routes(); ++v) {
        auto const& route = best.route(v);
        if (route.empty())
            continue;

        std::vector<int> client_ids;
        client_ids.reserve(route.size());
        for (int i = 0; i < route.size(); ++i) {
            client_ids.push_back(route.client(i));
        }
        result.routes_.push_back(std::move(client_ids));
    }

    // Extract unserved clients.
    for (int c : best.unassigned()) {
        result.unserved_.push_back(c);
    }

    result.iterations_ = stop.iterations();

    auto wall_end = std::chrono::steady_clock::now();
    result.elapsed_seconds_ = std::chrono::duration<double>(
        wall_end - wall_start).count();

    return result;
}

// ---------------------------------------------------------------------------
//  Free function: solve from file
// ---------------------------------------------------------------------------

Result solve(const std::string& instance_path, TimeLimit tl)
{
    // Try to read the VRP instance file.
    VrpInstance inst;
    try {
        inst = read_vrp(instance_path);
    } catch (std::runtime_error const&) {
        // File not found or parse error -- return empty result.
        return {};
    }

    RoutingModel model;

    // Add depots.  If no depot section was present, default to node 0.
    std::vector<int> depot_ids = inst.depot_ids;
    if (depot_ids.empty()) {
        depot_ids.push_back(0);
    }

    // We need to map from VRP instance node indices (0..dimension-1) to
    // the RoutingModel node numbering (depots first, then clients).
    //
    // depot_set[i] = true if node i is a depot.
    std::vector<bool> is_depot(inst.dimension, false);
    for (int d : depot_ids) {
        is_depot[d] = true;
    }

    // Add depots to the model.
    for (int d : depot_ids) {
        if (!inst.coords.empty()) {
            model.add_depot(inst.coords[d].x, inst.coords[d].y);
        } else {
            model.add_depot(d);
        }
    }

    // Add clients (all non-depot nodes).
    // Track the mapping: vrp_node -> client index in the model.
    std::vector<int> node_to_model(inst.dimension, -1);
    int num_depots = static_cast<int>(depot_ids.size());

    // Map depot nodes first.
    for (int i = 0; i < num_depots; ++i) {
        node_to_model[depot_ids[i]] = i;  // depot index
    }

    int client_idx = 0;
    for (int i = 0; i < inst.dimension; ++i) {
        if (is_depot[i])
            continue;

        ClientParams cp;
        if (!inst.demands.empty() && inst.demands[i] > 0) {
            cp.demand = {inst.demands[i]};
        }

        if (!inst.coords.empty()) {
            model.add_client(inst.coords[i].x, inst.coords[i].y, cp);
        } else {
            model.add_client(i, cp);
        }

        // Client index in model numbering is (num_depots + client_idx).
        node_to_model[i] = num_depots + client_idx;
        ++client_idx;
    }

    // Add vehicle type. Use the number of vehicles from the instance if
    // available; otherwise estimate from capacity.
    int num_vehicles = inst.vehicles;
    if (num_vehicles <= 0) {
        // Estimate: ceil(total_demand / capacity), with a minimum of 1.
        int total_demand = 0;
        for (int i = 0; i < inst.dimension; ++i) {
            if (!is_depot[i] && !inst.demands.empty())
                total_demand += inst.demands[i];
        }
        if (inst.capacity > 0) {
            num_vehicles = (total_demand + inst.capacity - 1) / inst.capacity;
        }
        // Ensure at least as many vehicles as clients (upper bound).
        num_vehicles = std::max(num_vehicles, client_idx);
    }

    VehicleTypeParams vtp;
    if (inst.capacity > 0) {
        vtp.capacity = {inst.capacity};
    }
    model.add_vehicle_type(num_vehicles, vtp);

    // Set explicit distances if available, or if computed from coordinates.
    // The ProblemData::Builder computes Euclidean distances from coordinates
    // by default. For EXPLICIT instances, we need to set distances manually.
    // For other distance types (GEO, ATT), we also need to set them since
    // the builder only computes EUC_2D.
    if (inst.edge_weight_type == EdgeWeightType::EXPLICIT ||
        inst.edge_weight_type == EdgeWeightType::GEO ||
        inst.edge_weight_type == EdgeWeightType::ATT ||
        inst.edge_weight_type == EdgeWeightType::CEIL_2D) {
        int n = num_depots + client_idx;  // total model nodes
        for (int i = 0; i < inst.dimension; ++i) {
            int mi = node_to_model[i];
            if (mi < 0) continue;
            for (int j = 0; j < inst.dimension; ++j) {
                int mj = node_to_model[j];
                if (mj < 0) continue;
                int d = inst.dist(i, j);
                model.set_distance(mi, mj, d);
            }
        }
    }

    return model.solve(tl);
}

} // namespace coso
