#include "checks.h"
#include "model/assignment_model.h"
#include "model/lotsizing_model.h"
#include "model/network_model.h"
#include "model/packing_model.h"
#include "model/routing_model.h"
#include "model/schedule_model.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

struct Scenario {
    std::string id;
    std::string model;
    double seconds = 1.0;
    double work_units = 0.0;
    std::set<std::string> checks;
};

std::string read_file(std::filesystem::path const& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Cannot open scenario file: " + path.string());
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::optional<std::string> extract_string(std::string const& text, std::string const& key) {
    std::regex re("\"" + key + "\"\\s*:\\s*\"([^\"]+)\"");
    std::smatch m;
    if (std::regex_search(text, m, re) && m.size() >= 2) {
        return m[1].str();
    }
    return std::nullopt;
}

std::optional<double> extract_number(std::string const& text, std::string const& key) {
    std::regex re("\"" + key + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
    std::smatch m;
    if (std::regex_search(text, m, re) && m.size() >= 2) {
        return std::stod(m[1].str());
    }
    return std::nullopt;
}

std::set<std::string> extract_checks(std::string const& text) {
    std::set<std::string> checks;
    std::regex arr_re("\"checks\"\\s*:\\s*\\[([^\\]]*)\\]");
    std::smatch arr;
    if (!std::regex_search(text, arr, arr_re) || arr.size() < 2) {
        checks.insert("nonnegative_cost");
        return checks;
    }

    std::string inside = arr[1].str();
    std::regex value_re("\"([^\"]+)\"");
    auto begin = std::sregex_iterator(inside.begin(), inside.end(), value_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        checks.insert((*it)[1].str());
    }
    if (checks.empty()) {
        checks.insert("nonnegative_cost");
    }
    return checks;
}

Scenario parse_scenario(std::filesystem::path const& path) {
    std::string text = read_file(path);

    Scenario s;
    s.id = extract_string(text, "id").value_or("");
    s.model = extract_string(text, "model").value_or("");
    s.seconds = extract_number(text, "seconds").value_or(1.0);
    s.work_units = extract_number(text, "work_units").value_or(0.0);
    s.checks = extract_checks(text);

    if (s.id.empty()) {
        throw std::runtime_error("Scenario parse error: missing string field 'id'");
    }
    if (s.model.empty()) {
        throw std::runtime_error("Scenario parse error: missing string field 'model'");
    }
    return s;
}

coso::Result solve_once(Scenario const& s) {
    coso::TimeLimit tl(s.seconds, s.work_units);

    if (s.model == "routing") {
        coso::RoutingModel m;
        m.add_depot(0.0, 0.0);
        m.add_vehicle_type(1, {.capacity = {10}});
        m.add_client(1.0, 0.0, {.demand = {1}});
        return m.solve(tl);
    }

    if (s.model == "network") {
        coso::NetworkModel m;
        int src = m.add_node(5, "src");
        int dst = m.add_node(-5, "dst");
        m.add_arc(src, dst, 2, 0, 5);
        return m.solve(tl);
    }

    if (s.model == "lotsizing") {
        coso::LotSizingModel m;
        m.set_num_periods(3);
        int p = m.add_product(100.0, 2.0, 1.0, 2.0);
        m.set_demand(p, 0, 10.0);
        m.set_demand(p, 1, 15.0);
        m.set_demand(p, 2, 20.0);
        m.set_capacity(0, 80.0);
        m.set_capacity(1, 80.0);
        m.set_capacity(2, 80.0);
        return m.solve(tl);
    }

    if (s.model == "schedule") {
        coso::ScheduleModel m;
        m.add_machine();
        int j = m.add_job();
        m.add_operation(j, {.machine = 0, .duration = 3});
        return m.solve(tl);
    }

    if (s.model == "assignment") {
        coso::AssignmentModel m;
        int day = m.add_shift_type({.name = "Day"});
        m.add_employee({.name = "Alice"});
        m.add_employee({.name = "Bob"});
        m.set_horizon(4);
        m.add_demand(day, {.min_employees = 1, .max_employees = 1});
        return m.solve(tl);
    }

    if (s.model == "packing") {
        coso::PackingModel m;
        m.add_bin_type({.capacity = {10}});
        m.add_item({.size = {6}});
        m.add_item({.size = {4}});
        m.add_item({.size = {3}});
        return m.solve(tl);
    }

    throw std::runtime_error("Unsupported model type: " + s.model);
}

std::string bool_json(bool v) {
    return v ? "true" : "false";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: e2e_runner <scenario.json>\n";
        return 1;
    }

    try {
        Scenario scenario = parse_scenario(argv[1]);
        coso::e2e::Evaluation const eval = coso::e2e::evaluate_checks(
            scenario.checks, [&scenario]() { return solve_once(scenario); });

        std::cout << "{\n"
                  << "  \"scenario_id\": \"" << scenario.id << "\",\n"
                  << "  \"model\": \"" << scenario.model << "\",\n"
                  << "  \"pass\": " << bool_json(eval.checks.pass) << ",\n"
                  << "  \"result\": {\n"
                  << "    \"feasible\": " << bool_json(eval.result.feasible()) << ",\n"
                  << "    \"cost\": " << eval.result.cost() << ",\n"
                  << "    \"work_ticks\": " << eval.result.work_ticks() << ",\n"
                  << "    \"work_units\": " << eval.result.work_units() << ",\n"
                  << "    \"elapsed_seconds\": " << eval.result.elapsed_seconds() << "\n"
                  << "  },\n"
                  << "  \"checks\": {\n"
                  << "    \"feasible\": " << bool_json(eval.checks.feasible) << ",\n"
                  << "    \"nonnegative_cost\": " << bool_json(eval.checks.nonnegative_cost)
                  << ",\n"
                  << "    \"deterministic_work\": " << bool_json(eval.checks.deterministic_work)
                  << "\n"
                  << "  },\n"
                  << "  \"errors\": [";
        for (std::size_t i = 0; i < eval.checks.errors.size(); ++i) {
            if (i > 0) {
                std::cout << ", ";
            }
            std::cout << "\"" << eval.checks.errors[i] << "\"";
        }
        std::cout << "]\n"
                  << "}\n";

        return eval.checks.pass ? 0 : 2;
    } catch (std::exception const& e) {
        std::cerr << "e2e_runner error: " << e.what() << "\n";
        return 1;
    }
}
