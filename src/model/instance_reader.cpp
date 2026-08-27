#include "model/instance_reader.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace coso {

namespace {

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return {};
    }
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
    return s;
}

std::optional<std::pair<std::string, std::string>> parse_key_value(const std::string& line) {
    auto pos = line.find(':');
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    auto key = trim(line.substr(0, pos));
    auto value = trim(line.substr(pos + 1));
    return std::make_pair(to_upper(key), value);
}

EdgeWeightType parse_edge_weight_type(const std::string& s) {
    auto upper = to_upper(trim(s));
    if (upper == "EUC_2D") {
        return EdgeWeightType::EUC_2D;
    }
    if (upper == "CEIL_2D") {
        return EdgeWeightType::CEIL_2D;
    }
    if (upper == "GEO") {
        return EdgeWeightType::GEO;
    }
    if (upper == "ATT") {
        return EdgeWeightType::ATT;
    }
    if (upper == "EXPLICIT") {
        return EdgeWeightType::EXPLICIT;
    }
    throw std::runtime_error("Unknown EDGE_WEIGHT_TYPE: " + s);
}

EdgeWeightFormat parse_edge_weight_format(const std::string& s) {
    auto upper = to_upper(trim(s));
    if (upper == "FULL_MATRIX") {
        return EdgeWeightFormat::FULL_MATRIX;
    }
    if (upper == "UPPER_ROW") {
        return EdgeWeightFormat::UPPER_ROW;
    }
    if (upper == "LOWER_ROW") {
        return EdgeWeightFormat::LOWER_ROW;
    }
    if (upper == "UPPER_DIAG_ROW") {
        return EdgeWeightFormat::UPPER_DIAG_ROW;
    }
    if (upper == "LOWER_DIAG_ROW") {
        return EdgeWeightFormat::LOWER_DIAG_ROW;
    }
    if (upper == "UPPER_COL") {
        return EdgeWeightFormat::UPPER_COL;
    }
    if (upper == "LOWER_COL") {
        return EdgeWeightFormat::LOWER_COL;
    }
    if (upper == "UPPER_DIAG_COL") {
        return EdgeWeightFormat::UPPER_DIAG_COL;
    }
    if (upper == "LOWER_DIAG_COL") {
        return EdgeWeightFormat::LOWER_DIAG_COL;
    }
    throw std::runtime_error("Unknown EDGE_WEIGHT_FORMAT: " + s);
}

std::vector<int> read_ints(std::istream& in, int count) {
    std::vector<int> values;
    values.reserve(count);
    int v;
    while (static_cast<int>(values.size()) < count && in >> v) {
        values.push_back(v);
    }
    std::string rest;
    if (in) {
        std::getline(in, rest);
    }
    return values;
}

void parse_edge_weight_section(std::istream& in, VrpInstance& inst) {
    int n = inst.dimension;
    inst.distance_matrix.assign(n * n, 0);

    if (!inst.edge_weight_format.has_value()) {
        throw std::runtime_error("EDGE_WEIGHT_SECTION requires EDGE_WEIGHT_FORMAT");
    }

    auto fmt = *inst.edge_weight_format;

    int expected = 0;
    switch (fmt) {
        case EdgeWeightFormat::FULL_MATRIX:
            expected = n * n;
            break;
        case EdgeWeightFormat::UPPER_ROW:
        case EdgeWeightFormat::LOWER_ROW:
        case EdgeWeightFormat::UPPER_COL:
        case EdgeWeightFormat::LOWER_COL:
            expected = n * (n - 1) / 2;
            break;
        case EdgeWeightFormat::UPPER_DIAG_ROW:
        case EdgeWeightFormat::LOWER_DIAG_ROW:
        case EdgeWeightFormat::UPPER_DIAG_COL:
        case EdgeWeightFormat::LOWER_DIAG_COL:
            expected = n * (n + 1) / 2;
            break;
    }

    auto values = read_ints(in, expected);
    if (static_cast<int>(values.size()) != expected) {
        throw std::runtime_error("EDGE_WEIGHT_SECTION: expected " + std::to_string(expected) +
                                 " values, got " + std::to_string(values.size()));
    }

    int idx = 0;
    switch (fmt) {
        case EdgeWeightFormat::FULL_MATRIX:
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < n; ++j) {
                    inst.distance_matrix[i * n + j] = values[idx++];
                }
            }
            break;
        case EdgeWeightFormat::UPPER_ROW:
            for (int i = 0; i < n; ++i) {
                for (int j = i + 1; j < n; ++j) {
                    inst.distance_matrix[i * n + j] = values[idx];
                    inst.distance_matrix[j * n + i] = values[idx];
                    ++idx;
                }
            }
            break;
        case EdgeWeightFormat::LOWER_ROW:
            for (int i = 1; i < n; ++i) {
                for (int j = 0; j < i; ++j) {
                    inst.distance_matrix[i * n + j] = values[idx];
                    inst.distance_matrix[j * n + i] = values[idx];
                    ++idx;
                }
            }
            break;
        case EdgeWeightFormat::UPPER_DIAG_ROW:
            for (int i = 0; i < n; ++i) {
                for (int j = i; j < n; ++j) {
                    inst.distance_matrix[i * n + j] = values[idx];
                    inst.distance_matrix[j * n + i] = values[idx];
                    ++idx;
                }
            }
            break;
        case EdgeWeightFormat::LOWER_DIAG_ROW:
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j <= i; ++j) {
                    inst.distance_matrix[i * n + j] = values[idx];
                    inst.distance_matrix[j * n + i] = values[idx];
                    ++idx;
                }
            }
            break;
        case EdgeWeightFormat::UPPER_COL:
            for (int j = 1; j < n; ++j) {
                for (int i = 0; i < j; ++i) {
                    inst.distance_matrix[i * n + j] = values[idx];
                    inst.distance_matrix[j * n + i] = values[idx];
                    ++idx;
                }
            }
            break;
        case EdgeWeightFormat::LOWER_COL:
            for (int j = 0; j < n - 1; ++j) {
                for (int i = j + 1; i < n; ++i) {
                    inst.distance_matrix[i * n + j] = values[idx];
                    inst.distance_matrix[j * n + i] = values[idx];
                    ++idx;
                }
            }
            break;
        case EdgeWeightFormat::UPPER_DIAG_COL:
            for (int j = 0; j < n; ++j) {
                for (int i = 0; i <= j; ++i) {
                    inst.distance_matrix[i * n + j] = values[idx];
                    inst.distance_matrix[j * n + i] = values[idx];
                    ++idx;
                }
            }
            break;
        case EdgeWeightFormat::LOWER_DIAG_COL:
            for (int j = 0; j < n; ++j) {
                for (int i = j; i < n; ++i) {
                    inst.distance_matrix[i * n + j] = values[idx];
                    inst.distance_matrix[j * n + i] = values[idx];
                    ++idx;
                }
            }
            break;
    }
}

void parse_node_coord_section(std::istream& in, VrpInstance& inst) {
    inst.coords.resize(inst.dimension);
    for (int k = 0; k < inst.dimension; ++k) {
        int id;
        double x, y;
        if (!(in >> id >> x >> y)) {
            throw std::runtime_error("NODE_COORD_SECTION: failed to read node " +
                                     std::to_string(k));
        }
        int idx = id - 1;
        if (idx < 0 || idx >= inst.dimension) {
            throw std::runtime_error("NODE_COORD_SECTION: node id " + std::to_string(id) +
                                     " out of range");
        }
        inst.coords[idx] = {x, y};
    }
    std::string rest;
    if (in) {
        std::getline(in, rest);
    }
}

void parse_demand_section(std::istream& in, VrpInstance& inst) {
    inst.demands.resize(inst.dimension, 0);
    for (int k = 0; k < inst.dimension; ++k) {
        int id, demand;
        if (!(in >> id >> demand)) {
            throw std::runtime_error("DEMAND_SECTION: failed to read entry " + std::to_string(k));
        }
        int idx = id - 1;
        if (idx < 0 || idx >= inst.dimension) {
            throw std::runtime_error("DEMAND_SECTION: node id " + std::to_string(id) +
                                     " out of range");
        }
        inst.demands[idx] = demand;
    }
    std::string rest;
    if (in) {
        std::getline(in, rest);
    }
}

void parse_depot_section(std::istream& in, VrpInstance& inst) {
    int id;
    while (in >> id) {
        if (id == -1) {
            break;
        }
        inst.depot_ids.push_back(id - 1);
    }
    std::string rest;
    if (in) {
        std::getline(in, rest);
    }
}

}  // anonymous namespace

// -- VrpInstance methods -----------------------------------------------------

int VrpInstance::euclidean_distance(int i, int j) const {
    if (coords.empty() || i < 0 || i >= static_cast<int>(coords.size()) || j < 0 ||
        j >= static_cast<int>(coords.size())) {
        return -1;
    }

    double dx = coords[i].x - coords[j].x;
    double dy = coords[i].y - coords[j].y;
    double d = std::sqrt(dx * dx + dy * dy);

    if (edge_weight_type == EdgeWeightType::CEIL_2D) {
        return static_cast<int>(std::ceil(d));
    }
    return static_cast<int>(std::round(d));
}

int VrpInstance::dist(int i, int j) const {
    if (edge_weight_type == EdgeWeightType::EXPLICIT) {
        if (distance_matrix.empty()) {
            return -1;
        }
        return distance_matrix[i * dimension + j];
    }

    if (edge_weight_type == EdgeWeightType::GEO) {
        if (coords.empty()) {
            return -1;
        }
        constexpr double kPi = 3.141592653589793;
        constexpr double kEarthRadiusKm = 6378.388;
        auto to_geo = [](double val) {
            int deg = static_cast<int>(val);
            double min_part = val - deg;
            return kPi * (deg + 5.0 * min_part / 3.0) / 180.0;
        };
        double lat_i = to_geo(coords[i].x), lon_i = to_geo(coords[i].y);
        double lat_j = to_geo(coords[j].x), lon_j = to_geo(coords[j].y);
        double q1 = std::cos(lon_i - lon_j);
        double q2 = std::cos(lat_i - lat_j);
        double q3 = std::cos(lat_i + lat_j);
        return static_cast<int>(
            kEarthRadiusKm * std::acos(0.5 * ((1.0 + q1) * q2 - (1.0 - q1) * q3)) + 1.0);
    }

    if (edge_weight_type == EdgeWeightType::ATT) {
        if (coords.empty()) {
            return -1;
        }
        double dx = coords[i].x - coords[j].x;
        double dy = coords[i].y - coords[j].y;
        double r = std::sqrt((dx * dx + dy * dy) / 10.0);
        int t = static_cast<int>(std::round(r));
        return (t < r) ? t + 1 : t;
    }

    return euclidean_distance(i, j);
}

// -- CVRPLIB .vrp parsing ----------------------------------------------------

VrpInstance parse_vrp(const std::string& content) {
    std::istringstream in(content);
    VrpInstance inst;
    std::string line;

    while (std::getline(in, line)) {
        auto trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }
        auto upper = to_upper(trimmed);

        if (upper == "NODE_COORD_SECTION") {
            parse_node_coord_section(in, inst);
            continue;
        }
        if (upper == "DEMAND_SECTION") {
            parse_demand_section(in, inst);
            continue;
        }
        if (upper == "DEPOT_SECTION") {
            parse_depot_section(in, inst);
            continue;
        }
        if (upper == "EDGE_WEIGHT_SECTION") {
            parse_edge_weight_section(in, inst);
            continue;
        }
        if (upper == "EOF") {
            break;
        }

        auto kv = parse_key_value(line);
        if (!kv) {
            continue;
        }
        auto& [key, value] = *kv;

        if (key == "NAME") {
            inst.name = value;
        } else if (key == "COMMENT") {
            inst.comment = value;
        } else if (key == "TYPE") {
            inst.type = to_upper(value);
        } else if (key == "DIMENSION") {
            inst.dimension = std::stoi(value);
        } else if (key == "CAPACITY") {
            inst.capacity = std::stoi(value);
        } else if (key == "VEHICLES") {
            inst.vehicles = std::stoi(value);
        } else if (key == "DISTANCE") {
            inst.distance = std::stod(value);
        } else if (key == "EDGE_WEIGHT_TYPE") {
            inst.edge_weight_type = parse_edge_weight_type(value);
        } else if (key == "EDGE_WEIGHT_FORMAT") {
            inst.edge_weight_format = parse_edge_weight_format(value);
        }
    }

    if (inst.dimension <= 0) {
        throw std::runtime_error("VRP parse error: DIMENSION not set or <= 0");
    }
    if (inst.edge_weight_type != EdgeWeightType::EXPLICIT && inst.coords.empty()) {
        throw std::runtime_error(
            "VRP parse error: non-EXPLICIT instance requires NODE_COORD_SECTION");
    }
    if (inst.edge_weight_type == EdgeWeightType::EXPLICIT && inst.distance_matrix.empty()) {
        throw std::runtime_error("VRP parse error: EXPLICIT instance requires EDGE_WEIGHT_SECTION");
    }

    return inst;
}

VrpInstance read_vrp(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open VRP file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return parse_vrp(ss.str());
}

// -- Solomon VRPTW parser ----------------------------------------------------

VrpInstance read_solomon(std::istream& input) {
    VrpInstance inst;
    inst.type = "VRPTW";
    inst.edge_weight_type = EdgeWeightType::EUC_2D;
    std::string line;

    if (!std::getline(input, line)) {
        throw std::runtime_error("Solomon parse error: empty input");
    }
    inst.name = trim(line);

    bool found_vehicle_data = false;
    while (std::getline(input, line)) {
        auto trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }
        auto upper = to_upper(trimmed);
        if (upper == "VEHICLE" || upper.starts_with("NUMBER")) {
            continue;
        }
        std::istringstream iss(trimmed);
        int nv = 0, cap = 0;
        if (iss >> nv >> cap) {
            inst.vehicles = nv;
            inst.capacity = cap;
            found_vehicle_data = true;
            break;
        }
    }
    if (!found_vehicle_data) {
        throw std::runtime_error("Solomon parse error: could not find vehicle data");
    }

    while (std::getline(input, line)) {
        auto trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }
        auto upper = to_upper(trimmed);
        if (upper == "CUSTOMER" || upper.find("CUST") != std::string::npos ||
            upper.find("XCOORD") != std::string::npos || upper.find("COORD") != std::string::npos) {
            continue;
        }

        std::istringstream iss(trimmed);
        int id = 0;
        double x = 0, y = 0;
        int demand = 0, ready = 0, due = 0, service = 0;
        if (!(iss >> id >> x >> y >> demand >> ready >> due >> service)) {
            continue;
        }
        inst.coords.push_back({x, y});
        inst.demands.push_back(demand);
        inst.time_windows.push_back({ready, due});
        inst.service_times.push_back(service);
        break;
    }

    while (std::getline(input, line)) {
        auto trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }
        if (to_upper(trimmed) == "EOF") {
            break;
        }
        std::istringstream iss(trimmed);
        int id = 0;
        double x = 0, y = 0;
        int demand = 0, ready = 0, due = 0, service = 0;
        if (!(iss >> id >> x >> y >> demand >> ready >> due >> service)) {
            break;
        }
        inst.coords.push_back({x, y});
        inst.demands.push_back(demand);
        inst.time_windows.push_back({ready, due});
        inst.service_times.push_back(service);
    }

    inst.dimension = static_cast<int>(inst.coords.size());
    if (inst.dimension <= 0) {
        throw std::runtime_error("Solomon parse error: no customer data found");
    }
    inst.depot_ids.push_back(0);
    return inst;
}

VrpInstance parse_solomon(const std::string& content) {
    std::istringstream in(content);
    return read_solomon(in);
}

VrpInstance read_solomon(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open Solomon file: " + path);
    }
    return read_solomon(file);
}

// -- Li-Lim PDPTW parser -----------------------------------------------------

VrpInstance read_lilim(std::istream& input) {
    VrpInstance inst;
    inst.type = "PDPTW";
    inst.edge_weight_type = EdgeWeightType::EUC_2D;
    std::string line;

    while (std::getline(input, line)) {
        auto trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }
        std::istringstream iss(trimmed);
        int nv = 0, cap = 0;
        if (!(iss >> nv >> cap)) {
            throw std::runtime_error("Li-Lim parse error: could not read vehicle data");
        }
        inst.vehicles = nv;
        inst.capacity = cap;
        break;
    }

    struct RawNode {
        int id = 0;
        double x = 0, y = 0;
        int demand = 0, earliest = 0, latest = 0, service = 0;
        int pickup_idx = 0, delivery_idx = 0;
    };
    std::vector<RawNode> nodes;

    while (std::getline(input, line)) {
        auto trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }
        if (to_upper(trimmed) == "EOF") {
            break;
        }
        std::istringstream iss(trimmed);
        RawNode node;
        if (!(iss >> node.id >> node.x >> node.y >> node.demand >> node.earliest >> node.latest >>
              node.service >> node.pickup_idx >> node.delivery_idx)) {
            break;
        }
        nodes.push_back(node);
    }

    if (nodes.empty()) {
        throw std::runtime_error("Li-Lim parse error: no node data found");
    }

    inst.dimension = static_cast<int>(nodes.size());
    inst.name = "lilim-" + std::to_string(inst.dimension);
    inst.coords.resize(inst.dimension);
    inst.demands.resize(inst.dimension);
    inst.time_windows.resize(inst.dimension);
    inst.service_times.resize(inst.dimension);

    int max_id = 0;
    for (auto const& n : nodes) {
        if (n.id > max_id) {
            max_id = n.id;
        }
    }
    std::vector<int> id_to_index(max_id + 1, -1);

    for (int i = 0; i < inst.dimension; ++i) {
        auto const& n = nodes[i];
        if (n.id >= 0 && n.id <= max_id) {
            id_to_index[n.id] = i;
        }
        inst.coords[i] = {n.x, n.y};
        inst.demands[i] = n.demand;
        inst.time_windows[i] = {n.earliest, n.latest};
        inst.service_times[i] = n.service;
    }

    inst.depot_ids.push_back(0);

    for (int i = 0; i < inst.dimension; ++i) {
        auto const& n = nodes[i];
        if (n.demand > 0 && n.delivery_idx > 0) {
            int delivery = -1;
            if (n.delivery_idx <= max_id) {
                delivery = id_to_index[n.delivery_idx];
            }
            if (delivery >= 0 && delivery < inst.dimension) {
                inst.pickup_delivery_pairs.push_back({i, delivery});
            }
        }
    }

    return inst;
}

VrpInstance parse_lilim(const std::string& content) {
    std::istringstream in(content);
    return read_lilim(in);
}

VrpInstance read_lilim(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open Li-Lim file: " + path);
    }
    return read_lilim(file);
}

}  // namespace coso
