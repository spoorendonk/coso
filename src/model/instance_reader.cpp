#include "model/instance_reader.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace coso {

namespace {

/// Trim leading and trailing whitespace from a string.
std::string trim(const std::string& s)
{
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/// Convert a string to uppercase.
std::string to_upper(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return s;
}

/// Split key: value lines. Returns the trimmed value after the colon.
/// If no colon is found, returns an empty optional.
std::optional<std::pair<std::string, std::string>>
parse_key_value(const std::string& line)
{
    auto pos = line.find(':');
    if (pos == std::string::npos) return std::nullopt;

    auto key   = trim(line.substr(0, pos));
    auto value = trim(line.substr(pos + 1));
    return std::make_pair(to_upper(key), value);
}

EdgeWeightType parse_edge_weight_type(const std::string& s)
{
    auto upper = to_upper(trim(s));
    if (upper == "EUC_2D")   return EdgeWeightType::EUC_2D;
    if (upper == "CEIL_2D")  return EdgeWeightType::CEIL_2D;
    if (upper == "GEO")      return EdgeWeightType::GEO;
    if (upper == "ATT")      return EdgeWeightType::ATT;
    if (upper == "EXPLICIT") return EdgeWeightType::EXPLICIT;
    throw std::runtime_error("Unknown EDGE_WEIGHT_TYPE: " + s);
}

EdgeWeightFormat parse_edge_weight_format(const std::string& s)
{
    auto upper = to_upper(trim(s));
    if (upper == "FULL_MATRIX")    return EdgeWeightFormat::FULL_MATRIX;
    if (upper == "UPPER_ROW")      return EdgeWeightFormat::UPPER_ROW;
    if (upper == "LOWER_ROW")      return EdgeWeightFormat::LOWER_ROW;
    if (upper == "UPPER_DIAG_ROW") return EdgeWeightFormat::UPPER_DIAG_ROW;
    if (upper == "LOWER_DIAG_ROW") return EdgeWeightFormat::LOWER_DIAG_ROW;
    if (upper == "UPPER_COL")      return EdgeWeightFormat::UPPER_COL;
    if (upper == "LOWER_COL")      return EdgeWeightFormat::LOWER_COL;
    if (upper == "UPPER_DIAG_COL") return EdgeWeightFormat::UPPER_DIAG_COL;
    if (upper == "LOWER_DIAG_COL") return EdgeWeightFormat::LOWER_DIAG_COL;
    throw std::runtime_error("Unknown EDGE_WEIGHT_FORMAT: " + s);
}

/// Read all integers from lines until we hit a section keyword or EOF.
std::vector<int> read_ints(std::istream& in, int count)
{
    std::vector<int> values;
    values.reserve(count);
    int v;
    while (static_cast<int>(values.size()) < count && in >> v) {
        values.push_back(v);
    }
    // Consume rest of line after last value
    std::string rest;
    if (in) std::getline(in, rest);
    return values;
}

/// Parse the EDGE_WEIGHT_SECTION and fill a full n x n distance matrix.
void parse_edge_weight_section(std::istream& in, VrpInstance& inst)
{
    int n = inst.dimension;
    inst.distance_matrix.assign(n * n, 0);

    if (!inst.edge_weight_format.has_value()) {
        throw std::runtime_error(
            "EDGE_WEIGHT_SECTION requires EDGE_WEIGHT_FORMAT");
    }

    auto fmt = *inst.edge_weight_format;

    // Determine how many values to read.
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
        throw std::runtime_error(
            "EDGE_WEIGHT_SECTION: expected " + std::to_string(expected) +
            " values, got " + std::to_string(values.size()));
    }

    int idx = 0;
    switch (fmt) {
        case EdgeWeightFormat::FULL_MATRIX:
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    inst.distance_matrix[i * n + j] = values[idx++];
            break;

        case EdgeWeightFormat::UPPER_ROW:
            for (int i = 0; i < n; ++i)
                for (int j = i + 1; j < n; ++j) {
                    inst.distance_matrix[i * n + j] = values[idx];
                    inst.distance_matrix[j * n + i] = values[idx];
                    ++idx;
                }
            break;

        case EdgeWeightFormat::LOWER_ROW:
            for (int i = 1; i < n; ++i)
                for (int j = 0; j < i; ++j) {
                    inst.distance_matrix[i * n + j] = values[idx];
                    inst.distance_matrix[j * n + i] = values[idx];
                    ++idx;
                }
            break;

        case EdgeWeightFormat::UPPER_DIAG_ROW:
            for (int i = 0; i < n; ++i)
                for (int j = i; j < n; ++j) {
                    inst.distance_matrix[i * n + j] = values[idx];
                    inst.distance_matrix[j * n + i] = values[idx];
                    ++idx;
                }
            break;

        case EdgeWeightFormat::LOWER_DIAG_ROW:
            for (int i = 0; i < n; ++i)
                for (int j = 0; j <= i; ++j) {
                    inst.distance_matrix[i * n + j] = values[idx];
                    inst.distance_matrix[j * n + i] = values[idx];
                    ++idx;
                }
            break;

        case EdgeWeightFormat::UPPER_COL:
            for (int j = 1; j < n; ++j)
                for (int i = 0; i < j; ++i) {
                    inst.distance_matrix[i * n + j] = values[idx];
                    inst.distance_matrix[j * n + i] = values[idx];
                    ++idx;
                }
            break;

        case EdgeWeightFormat::LOWER_COL:
            for (int j = 0; j < n - 1; ++j)
                for (int i = j + 1; i < n; ++i) {
                    inst.distance_matrix[i * n + j] = values[idx];
                    inst.distance_matrix[j * n + i] = values[idx];
                    ++idx;
                }
            break;

        case EdgeWeightFormat::UPPER_DIAG_COL:
            for (int j = 0; j < n; ++j)
                for (int i = 0; i <= j; ++i) {
                    inst.distance_matrix[i * n + j] = values[idx];
                    inst.distance_matrix[j * n + i] = values[idx];
                    ++idx;
                }
            break;

        case EdgeWeightFormat::LOWER_DIAG_COL:
            for (int j = 0; j < n; ++j)
                for (int i = j; i < n; ++i) {
                    inst.distance_matrix[i * n + j] = values[idx];
                    inst.distance_matrix[j * n + i] = values[idx];
                    ++idx;
                }
            break;
    }
}

/// Parse NODE_COORD_SECTION: reads dimension lines of (id x y).
void parse_node_coord_section(std::istream& in, VrpInstance& inst)
{
    inst.coords.resize(inst.dimension);
    for (int k = 0; k < inst.dimension; ++k) {
        int id;
        double x, y;
        if (!(in >> id >> x >> y)) {
            throw std::runtime_error(
                "NODE_COORD_SECTION: failed to read node " +
                std::to_string(k));
        }
        // CVRPLIB uses 1-based ids; convert to 0-based.
        int idx = id - 1;
        if (idx < 0 || idx >= inst.dimension) {
            throw std::runtime_error(
                "NODE_COORD_SECTION: node id " + std::to_string(id) +
                " out of range [1, " + std::to_string(inst.dimension) + "]");
        }
        inst.coords[idx] = {x, y};
    }
    // Consume rest of line.
    std::string rest;
    if (in) std::getline(in, rest);
}

/// Parse DEMAND_SECTION: reads dimension lines of (id demand).
void parse_demand_section(std::istream& in, VrpInstance& inst)
{
    inst.demands.resize(inst.dimension, 0);
    for (int k = 0; k < inst.dimension; ++k) {
        int id, demand;
        if (!(in >> id >> demand)) {
            throw std::runtime_error(
                "DEMAND_SECTION: failed to read entry " + std::to_string(k));
        }
        int idx = id - 1;
        if (idx < 0 || idx >= inst.dimension) {
            throw std::runtime_error(
                "DEMAND_SECTION: node id " + std::to_string(id) +
                " out of range [1, " + std::to_string(inst.dimension) + "]");
        }
        inst.demands[idx] = demand;
    }
    std::string rest;
    if (in) std::getline(in, rest);
}

/// Parse DEPOT_SECTION: reads depot ids until -1 or EOF.
void parse_depot_section(std::istream& in, VrpInstance& inst)
{
    int id;
    while (in >> id) {
        if (id == -1) break;
        // Convert 1-based to 0-based.
        inst.depot_ids.push_back(id - 1);
    }
    // Consume rest of line.
    std::string rest;
    if (in) std::getline(in, rest);
}

} // anonymous namespace

// -- VrpInstance methods -----------------------------------------------------

int VrpInstance::euclidean_distance(int i, int j) const
{
    if (coords.empty() ||
        i < 0 || i >= static_cast<int>(coords.size()) ||
        j < 0 || j >= static_cast<int>(coords.size()))
        return -1;

    double dx = coords[i].x - coords[j].x;
    double dy = coords[i].y - coords[j].y;
    double d  = std::sqrt(dx * dx + dy * dy);

    if (edge_weight_type == EdgeWeightType::CEIL_2D)
        return static_cast<int>(std::ceil(d));

    // EUC_2D: round to nearest integer (nint)
    return static_cast<int>(std::round(d));
}

int VrpInstance::dist(int i, int j) const
{
    if (edge_weight_type == EdgeWeightType::EXPLICIT) {
        if (distance_matrix.empty()) return -1;
        return distance_matrix[i * dimension + j];
    }

    if (edge_weight_type == EdgeWeightType::GEO) {
        // Geographic distance (TSPLIB convention).
        if (coords.empty()) return -1;
        constexpr double PI = 3.141592653589793;
        constexpr double RRR = 6378.388;

        auto to_geo = [](double val) {
            int deg = static_cast<int>(val);
            double min_part = val - deg;
            return PI * (deg + 5.0 * min_part / 3.0) / 180.0;
        };

        double lat_i = to_geo(coords[i].x);
        double lon_i = to_geo(coords[i].y);
        double lat_j = to_geo(coords[j].x);
        double lon_j = to_geo(coords[j].y);

        double q1 = std::cos(lon_i - lon_j);
        double q2 = std::cos(lat_i - lat_j);
        double q3 = std::cos(lat_i + lat_j);
        return static_cast<int>(
            RRR * std::acos(0.5 * ((1.0 + q1) * q2 - (1.0 - q1) * q3)) +
            1.0);
    }

    if (edge_weight_type == EdgeWeightType::ATT) {
        // Pseudo-Euclidean (ATT instances).
        if (coords.empty()) return -1;
        double dx = coords[i].x - coords[j].x;
        double dy = coords[i].y - coords[j].y;
        double r  = std::sqrt((dx * dx + dy * dy) / 10.0);
        int t     = static_cast<int>(std::round(r));
        return (t < r) ? t + 1 : t;
    }

    // EUC_2D or CEIL_2D
    return euclidean_distance(i, j);
}

// -- Parsing -----------------------------------------------------------------

VrpInstance parse_vrp(const std::string& content)
{
    std::istringstream in(content);
    VrpInstance inst;
    std::string line;

    while (std::getline(in, line)) {
        auto trimmed = trim(line);
        if (trimmed.empty()) continue;

        auto upper = to_upper(trimmed);

        // Check for section headers (no colon).
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
        if (upper == "EOF") break;

        // Key: value lines.
        auto kv = parse_key_value(line);
        if (!kv) continue;

        auto& [key, value] = *kv;

        if (key == "NAME")               inst.name = value;
        else if (key == "COMMENT")        inst.comment = value;
        else if (key == "TYPE")           inst.type = to_upper(value);
        else if (key == "DIMENSION")      inst.dimension = std::stoi(value);
        else if (key == "CAPACITY")       inst.capacity = std::stoi(value);
        else if (key == "VEHICLES")       inst.vehicles = std::stoi(value);
        else if (key == "DISTANCE")       inst.distance = std::stod(value);
        else if (key == "EDGE_WEIGHT_TYPE")
            inst.edge_weight_type = parse_edge_weight_type(value);
        else if (key == "EDGE_WEIGHT_FORMAT")
            inst.edge_weight_format = parse_edge_weight_format(value);
        // Ignore unknown keys.
    }

    // Validate required fields.
    if (inst.dimension <= 0) {
        throw std::runtime_error("VRP parse error: DIMENSION not set or <= 0");
    }
    if (inst.edge_weight_type != EdgeWeightType::EXPLICIT &&
        inst.coords.empty()) {
        throw std::runtime_error(
            "VRP parse error: non-EXPLICIT instance requires "
            "NODE_COORD_SECTION");
    }
    if (inst.edge_weight_type == EdgeWeightType::EXPLICIT &&
        inst.distance_matrix.empty()) {
        throw std::runtime_error(
            "VRP parse error: EXPLICIT instance requires "
            "EDGE_WEIGHT_SECTION");
    }

    return inst;
}

VrpInstance read_vrp(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open VRP file: " + path);
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return parse_vrp(ss.str());
}

} // namespace coso
