#pragma once

#include "types.h"

#include <cmath>
#include <istream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace coso {

/// Edge weight type in a VRP instance file.
enum class EdgeWeightType {
    EUC_2D,       ///< Euclidean 2D (rounded to nearest int)
    CEIL_2D,      ///< Euclidean 2D (ceiling)
    GEO,          ///< Geographic (latitude/longitude)
    ATT,          ///< Pseudo-Euclidean (ATT instances)
    EXPLICIT,     ///< Distances given as a matrix
};

/// Edge weight format for EXPLICIT instances.
enum class EdgeWeightFormat {
    FULL_MATRIX,       ///< n x n matrix
    UPPER_ROW,         ///< Upper-triangular, row-by-row (no diagonal)
    LOWER_ROW,         ///< Lower-triangular, row-by-row (no diagonal)
    UPPER_DIAG_ROW,    ///< Upper-triangular with diagonal
    LOWER_DIAG_ROW,    ///< Lower-triangular with diagonal
    UPPER_COL,         ///< Upper-triangular, column-by-column
    LOWER_COL,         ///< Lower-triangular, column-by-column
    UPPER_DIAG_COL,    ///< Upper-triangular with diagonal, col-by-col
    LOWER_DIAG_COL,    ///< Lower-triangular with diagonal, col-by-col
};

/// Parsed data from a VRP instance file.
///
/// Contains all fields that can appear in a .vrp file or Solomon/Li-Lim
/// format: metadata, coordinates, demands, depot indices, vehicle capacity,
/// time windows, service times, pickup-delivery pairs, and optionally an
/// explicit distance matrix.
struct VrpInstance {
    // -- Metadata --
    std::string name;
    std::string comment;
    std::string type;                               ///< e.g. "CVRP", "VRPTW", "PDPTW"
    int dimension       = 0;                        ///< Number of nodes
    int capacity        = 0;                        ///< Vehicle capacity
    int vehicles        = 0;                        ///< Min vehicles (0 = unset)
    double distance     = 0.0;                      ///< Best known distance (0 = unset)

    EdgeWeightType edge_weight_type = EdgeWeightType::EUC_2D;
    std::optional<EdgeWeightFormat> edge_weight_format;

    // -- Node data (indexed 0..dimension-1) --
    std::vector<Coord> coords;                      ///< Node coordinates
    std::vector<int> demands;                       ///< Node demands
    std::vector<int> depot_ids;                     ///< Depot node indices (0-based)

    // -- Time windows (Solomon/VRPTW format) --
    std::vector<TimeWindow> time_windows;           ///< Per-node time windows
    std::vector<int> service_times;                 ///< Per-node service durations

    // -- Pickup-delivery pairs (Li-Lim/PDPTW format) --
    /// Each pair is (pickup_index, delivery_index), 0-based.
    std::vector<std::pair<int, int>> pickup_delivery_pairs;

    // -- Explicit distance matrix (only for EXPLICIT edge weight type) --
    /// Row-major n x n matrix. distances[i * dimension + j] = dist(i, j).
    std::vector<int> distance_matrix;

    /// Compute Euclidean distance between two nodes (for EUC_2D/CEIL_2D).
    /// Returns -1 if coordinates are not available.
    [[nodiscard]] int euclidean_distance(int i, int j) const;

    /// Get distance between nodes i and j, using either the explicit matrix
    /// or computed from coordinates depending on edge_weight_type.
    [[nodiscard]] int dist(int i, int j) const;
};

/// Read a Solomon VRPTW instance from the given path.
[[nodiscard]] VrpInstance read_solomon(const std::string& path);

/// Parse a Solomon VRPTW instance from a string.
[[nodiscard]] VrpInstance parse_solomon(const std::string& content);

/// Read a Solomon VRPTW instance from a stream.
[[nodiscard]] VrpInstance read_solomon(std::istream& input);

/// Read a Li-Lim PDPTW instance from the given path.
[[nodiscard]] VrpInstance read_lilim(const std::string& path);

/// Parse a Li-Lim PDPTW instance from a string.
[[nodiscard]] VrpInstance parse_lilim(const std::string& content);

/// Read a Li-Lim PDPTW instance from a stream.
[[nodiscard]] VrpInstance read_lilim(std::istream& input);

/// Read a CVRPLIB .vrp file from the given path.
[[nodiscard]] VrpInstance read_vrp(const std::string& path);

/// Parse a CVRPLIB .vrp instance from a string (useful for testing).
[[nodiscard]] VrpInstance parse_vrp(const std::string& content);

} // namespace coso
