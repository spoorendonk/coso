#include "model/lotsizing_model.h"
#include "model/network_model.h"
#include "model/routing_model.h"
#include "model/types.h"

#include <nanobind/nanobind.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <climits>

namespace nb = nanobind;
using namespace nb::literals;

NB_MODULE(_coso, m) {
    m.doc() = "COSO — Combinatorial Structure-aware Optimization";
    m.attr("__version__") = "0.1.0";

    // -- TimeLimit ------------------------------------------------------------

    nb::class_<coso::TimeLimit>(m, "TimeLimit")
        .def(nb::init<double, double>(), "seconds"_a, "work_units"_a = 0.0)
        .def_rw("seconds", &coso::TimeLimit::seconds)
        .def_rw("work_units", &coso::TimeLimit::work_units)
        .def("__repr__", [](const coso::TimeLimit& tl) {
            return "TimeLimit(seconds=" + std::to_string(tl.seconds) +
                   ", work_units=" + std::to_string(tl.work_units) + ")";
        });

    // -- Coord ----------------------------------------------------------------

    nb::class_<coso::Coord>(m, "Coord")
        .def(nb::init<double, double>(), "x"_a, "y"_a)
        .def_rw("x", &coso::Coord::x)
        .def_rw("y", &coso::Coord::y)
        .def("__repr__", [](const coso::Coord& c) {
            return "Coord(x=" + std::to_string(c.x) + ", y=" + std::to_string(c.y) + ")";
        });

    // -- TimeWindow -----------------------------------------------------------

    nb::class_<coso::TimeWindow>(m, "TimeWindow")
        .def(nb::init<int, int>(), "start"_a, "end"_a)
        .def_rw("start", &coso::TimeWindow::start)
        .def_rw("end", &coso::TimeWindow::end)
        .def("__repr__", [](const coso::TimeWindow& tw) {
            return "TimeWindow(start=" + std::to_string(tw.start) +
                   ", end=" + std::to_string(tw.end) + ")";
        });

    // -- CostParams -----------------------------------------------------------

    nb::class_<coso::CostParams>(m, "CostParams")
        .def(nb::init<>())
        .def_rw("fixed_cost", &coso::CostParams::fixed_cost)
        .def_rw("unit_distance_cost", &coso::CostParams::unit_distance_cost)
        .def_rw("unit_duration_cost", &coso::CostParams::unit_duration_cost)
        .def("__repr__", [](const coso::CostParams& cp) {
            return "CostParams(fixed=" + std::to_string(cp.fixed_cost) +
                   ", dist=" + std::to_string(cp.unit_distance_cost) +
                   ", dur=" + std::to_string(cp.unit_duration_cost) + ")";
        });

    // -- Result ---------------------------------------------------------------

    nb::class_<coso::Result::PathFlow>(m, "PathFlow")
        .def(nb::init<>())
        .def_rw("path", &coso::Result::PathFlow::path)
        .def_rw("flow", &coso::Result::PathFlow::flow);

    nb::class_<coso::Result>(m, "Result")
        .def_prop_ro("feasible", &coso::Result::feasible)
        .def_prop_ro("cost", &coso::Result::cost)
        .def_prop_ro("elapsed_seconds", &coso::Result::elapsed_seconds)
        .def_prop_ro("iterations", &coso::Result::iterations)
        .def_prop_ro("work_ticks", &coso::Result::work_ticks)
        .def_prop_ro("work_units", &coso::Result::work_units)
        .def_prop_ro("routes", &coso::Result::routes)
        .def_prop_ro("unserved", &coso::Result::unserved)
        .def_prop_ro("flows", &coso::Result::flows)
        .def_prop_ro("production", &coso::Result::production)
        .def_prop_ro("inventory", &coso::Result::inventory)
        .def("__repr__", [](const coso::Result& r) {
            return "Result(feasible=" + std::string(r.feasible() ? "True" : "False") +
                   ", cost=" + std::to_string(r.cost()) +
                   ", routes=" + std::to_string(r.routes().size()) +
                   ", elapsed=" + std::to_string(r.elapsed_seconds()) + "s" +
                   ", work=" + std::to_string(r.work_units()) + ")";
        });

    // -- VehicleTypeParams ----------------------------------------------------

    nb::class_<coso::VehicleTypeParams>(m, "VehicleTypeParams")
        .def(nb::init<>())
        .def_rw("capacity", &coso::VehicleTypeParams::capacity)
        .def_rw("max_duration", &coso::VehicleTypeParams::max_duration)
        .def_rw("max_distance", &coso::VehicleTypeParams::max_distance)
        .def_rw("min_tasks", &coso::VehicleTypeParams::min_tasks)
        .def_rw("max_tasks", &coso::VehicleTypeParams::max_tasks)
        .def_rw("max_overtime", &coso::VehicleTypeParams::max_overtime)
        .def_rw("unit_overtime_cost", &coso::VehicleTypeParams::unit_overtime_cost)
        .def_rw("reload_depot", &coso::VehicleTypeParams::reload_depot)
        .def_rw("max_reloads", &coso::VehicleTypeParams::max_reloads)
        .def_rw("cost", &coso::VehicleTypeParams::cost)
        .def_rw("profile", &coso::VehicleTypeParams::profile)
        .def_rw("skills", &coso::VehicleTypeParams::skills);

    // -- ClientParams ---------------------------------------------------------

    nb::class_<coso::ClientParams>(m, "ClientParams")
        .def(nb::init<>())
        .def_rw("demand", &coso::ClientParams::demand)
        .def_rw("pickup", &coso::ClientParams::pickup)
        .def_rw("tw", &coso::ClientParams::tw)
        .def_rw("extra_tw", &coso::ClientParams::extra_tw)
        .def_rw("service", &coso::ClientParams::service)
        .def_rw("release_time", &coso::ClientParams::release_time)
        .def_rw("prize", &coso::ClientParams::prize)
        .def_rw("required", &coso::ClientParams::required)
        .def_rw("group", &coso::ClientParams::group)
        .def_rw("skills", &coso::ClientParams::skills)
        .def_rw("client_type", &coso::ClientParams::client_type);

    // -- DepotParams ----------------------------------------------------------

    nb::class_<coso::DepotParams>(m, "DepotParams")
        .def(nb::init<>())
        .def_rw("tw", &coso::DepotParams::tw);

    // -- RoutingModel stored entries ------------------------------------------
    //
    //  Read-only views of what the model was told; returned by the accessors
    //  below.  Entries are copied out, so they never alias the model.

    nb::class_<coso::RoutingModel::DepotEntry>(m, "DepotEntry")
        .def_ro("x", &coso::RoutingModel::DepotEntry::x)
        .def_ro("y", &coso::RoutingModel::DepotEntry::y)
        .def_ro("has_coord", &coso::RoutingModel::DepotEntry::has_coord)
        .def_ro("explicit_id", &coso::RoutingModel::DepotEntry::explicit_id)
        .def_ro("params", &coso::RoutingModel::DepotEntry::params);

    nb::class_<coso::RoutingModel::ClientEntry>(m, "ClientEntry")
        .def_ro("x", &coso::RoutingModel::ClientEntry::x)
        .def_ro("y", &coso::RoutingModel::ClientEntry::y)
        .def_ro("has_coord", &coso::RoutingModel::ClientEntry::has_coord)
        .def_ro("explicit_id", &coso::RoutingModel::ClientEntry::explicit_id)
        .def_ro("params", &coso::RoutingModel::ClientEntry::params);

    nb::class_<coso::RoutingModel::VehicleTypeEntry>(m, "VehicleTypeEntry")
        .def_ro("count", &coso::RoutingModel::VehicleTypeEntry::count)
        .def_ro("params", &coso::RoutingModel::VehicleTypeEntry::params);

    nb::class_<coso::RoutingModel::MatEntry>(m, "MatEntry")
        .def_ro("profile", &coso::RoutingModel::MatEntry::profile)
        .def_ro("from_node", &coso::RoutingModel::MatEntry::from)
        .def_ro("to_node", &coso::RoutingModel::MatEntry::to)
        .def_ro("value", &coso::RoutingModel::MatEntry::value);

    // -- RoutingModel ---------------------------------------------------------

    nb::class_<coso::RoutingModel>(m, "RoutingModel")
        .def(nb::init<>())

        // add_depot: coordinate-based
        .def("add_depot",
             nb::overload_cast<double, double, coso::DepotParams>(&coso::RoutingModel::add_depot),
             "x"_a, "y"_a, "params"_a = coso::DepotParams{})

        // add_depot: explicit id
        .def("add_depot_id",
             nb::overload_cast<int, coso::DepotParams>(&coso::RoutingModel::add_depot), "id"_a,
             "params"_a = coso::DepotParams{})

        // add_vehicle_type
        .def("add_vehicle_type", &coso::RoutingModel::add_vehicle_type, "count"_a,
             "params"_a = coso::VehicleTypeParams{})

        // add_client: coordinate-based
        .def("add_client",
             nb::overload_cast<double, double, coso::ClientParams>(&coso::RoutingModel::add_client),
             "x"_a, "y"_a, "params"_a = coso::ClientParams{})

        // add_client: explicit id
        .def("add_client_id",
             nb::overload_cast<int, coso::ClientParams>(&coso::RoutingModel::add_client), "id"_a,
             "params"_a = coso::ClientParams{})

        // add_pickup / add_delivery / add_request
        .def("add_pickup",
             nb::overload_cast<double, double, coso::ClientParams>(&coso::RoutingModel::add_pickup),
             "x"_a, "y"_a, "params"_a = coso::ClientParams{})
        .def("add_delivery",
             nb::overload_cast<double, double, coso::ClientParams>(
                 &coso::RoutingModel::add_delivery),
             "x"_a, "y"_a, "params"_a = coso::ClientParams{})
        .def("add_request", &coso::RoutingModel::add_request, "pickup"_a, "delivery"_a)
        .def("add_pickup_delivery", &coso::RoutingModel::add_pickup_delivery, "pickup"_a,
             "delivery"_a)
        .def("add_client_group", &coso::RoutingModel::add_client_group)

        // Distance / duration matrices
        .def("set_distance", &coso::RoutingModel::set_distance, "from_node"_a, "to_node"_a,
             "dist"_a)
        .def("set_duration", &coso::RoutingModel::set_duration, "from_node"_a, "to_node"_a, "dur"_a)
        .def("set_profile", &coso::RoutingModel::set_profile, "profile"_a)
        .def("set_profile_distance", &coso::RoutingModel::set_profile_distance, "profile"_a,
             "from_node"_a, "to_node"_a, "dist"_a)
        .def("set_profile_duration", &coso::RoutingModel::set_profile_duration, "profile"_a,
             "from_node"_a, "to_node"_a, "dur"_a)
        .def("set_cost_matrix", &coso::RoutingModel::set_cost_matrix, "profile"_a, "from_node"_a,
             "to_node"_a, "cost"_a)

        // Warm start
        .def("set_initial_routes", &coso::RoutingModel::set_initial_routes, "routes"_a)
        .def("pin", &coso::RoutingModel::pin, "client_id"_a)

        // Solve
        .def("solve", &coso::RoutingModel::solve, "time_limit"_a)

        // Accessors
        .def("num_depots", &coso::RoutingModel::num_depots)
        .def("depot", &coso::RoutingModel::depot, "d"_a)
        .def("num_clients", &coso::RoutingModel::num_clients)
        .def("client", &coso::RoutingModel::client, "c"_a)
        .def("num_vehicle_types", &coso::RoutingModel::num_vehicle_types)
        .def("vehicle_type", &coso::RoutingModel::vehicle_type, "v"_a)
        .def("num_client_groups", &coso::RoutingModel::num_client_groups)
        .def("requests", &coso::RoutingModel::requests)
        .def("distance_entries", &coso::RoutingModel::distance_entries)
        .def("duration_entries", &coso::RoutingModel::duration_entries)
        .def("cost_entries", &coso::RoutingModel::cost_entries)
        .def("initial_routes", &coso::RoutingModel::initial_routes)
        .def("pinned", &coso::RoutingModel::pinned);

    // -- Free function: solve instance file -----------------------------------

    m.def("solve_instance", &coso::solve, "instance_path"_a, "time_limit"_a,
          "Solve a CVRPLIB/VRPLIB instance file directly.");

    // -- NetworkModel stored entries ------------------------------------------

    nb::class_<coso::NetworkModel::NodeEntry>(m, "NodeEntry")
        .def_ro("supply", &coso::NetworkModel::NodeEntry::supply)
        .def_ro("name", &coso::NetworkModel::NodeEntry::name);

    nb::class_<coso::NetworkModel::ArcEntry>(m, "ArcEntry")
        .def_ro("tail", &coso::NetworkModel::ArcEntry::tail)
        .def_ro("head", &coso::NetworkModel::ArcEntry::head)
        .def_ro("cost", &coso::NetworkModel::ArcEntry::cost)
        .def_ro("lower_cap", &coso::NetworkModel::ArcEntry::lower_cap)
        .def_ro("upper_cap", &coso::NetworkModel::ArcEntry::upper_cap);

    // -- NetworkModel ---------------------------------------------------------

    nb::class_<coso::NetworkModel>(m, "NetworkModel")
        .def(nb::init<>())
        .def("add_node", &coso::NetworkModel::add_node, "supply"_a = 0, "name"_a = "")
        .def("add_arc", &coso::NetworkModel::add_arc, "tail"_a, "head"_a, "cost"_a = 0,
             "lower_cap"_a = 0, "upper_cap"_a = INT_MAX)
        .def("solve", &coso::NetworkModel::solve, "time_limit"_a)

        // Accessors
        .def("num_nodes", &coso::NetworkModel::num_nodes)
        .def("node", &coso::NetworkModel::node, "n"_a)
        .def("num_arcs", &coso::NetworkModel::num_arcs)
        .def("arc", &coso::NetworkModel::arc, "a"_a);

    // -- LotSizingModel stored entries ----------------------------------------

    nb::class_<coso::LotSizingModel::ProductEntry>(m, "ProductEntry")
        .def_ro("setup_cost", &coso::LotSizingModel::ProductEntry::setup_cost)
        .def_ro("setup_time", &coso::LotSizingModel::ProductEntry::setup_time)
        .def_ro("unit_production_cost", &coso::LotSizingModel::ProductEntry::unit_production_cost)
        .def_ro("holding_cost", &coso::LotSizingModel::ProductEntry::holding_cost);

    nb::class_<coso::LotSizingModel::BomEntry>(m, "BomEntry")
        .def_ro("parent", &coso::LotSizingModel::BomEntry::parent)
        .def_ro("child", &coso::LotSizingModel::BomEntry::child)
        .def_ro("quantity", &coso::LotSizingModel::BomEntry::quantity);

    // -- LotSizingModel -------------------------------------------------------

    nb::class_<coso::LotSizingModel>(m, "LotSizingModel")
        .def(nb::init<>())
        .def("set_num_periods", &coso::LotSizingModel::set_num_periods, "periods"_a)
        .def("add_product", &coso::LotSizingModel::add_product, "setup_cost"_a, "setup_time"_a,
             "unit_production_cost"_a, "holding_cost"_a)
        .def("set_demand", &coso::LotSizingModel::set_demand, "product"_a, "period"_a, "demand"_a)
        .def("set_capacity", &coso::LotSizingModel::set_capacity, "period"_a, "capacity"_a)
        .def("add_bom", &coso::LotSizingModel::add_bom, "parent"_a, "child"_a, "quantity"_a = 1.0)
        .def("solve", &coso::LotSizingModel::solve, "time_limit"_a)

        // Accessors
        .def("num_periods", &coso::LotSizingModel::num_periods)
        .def("num_products", &coso::LotSizingModel::num_products)
        .def("product", &coso::LotSizingModel::product, "p"_a)
        .def("demands", &coso::LotSizingModel::demands)
        .def("capacities", &coso::LotSizingModel::capacities)
        .def("bom", &coso::LotSizingModel::bom);
}
