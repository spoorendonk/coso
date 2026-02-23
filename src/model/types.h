#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace coso {

/// 2D coordinate (e.g., depot or client location).
struct Coord {
    double x, y;
};

/// Time window constraint: service must start within [start, end].
struct TimeWindow {
    int start, end;
};

/// Cost parameters for a vehicle type.
struct CostParams {
    int fixed_cost          = 0;
    int unit_distance_cost  = 1;
    int unit_duration_cost  = 0;
    int per_task_hour_cost  = 0;
};

/// Stop criterion passed to solve().
struct TimeLimit {
    double seconds;
    explicit TimeLimit(double s) : seconds(s) {}
};

/// Common result fields shared across all engines.
///
/// Engine-specific data is stored in typed vectors and accessed through
/// convenience methods.  Only the relevant engine populates its section:
///   - routing:    routes(), unserved()
///   - scheduling: makespan(), schedule()
///   - assignment: assignments(), day()
///   - packing:    bins(), num_bins()
///   - network:    flows()
struct Result {
    bool   feasible        = false;
    double cost            = 0.0;
    double elapsed_seconds = 0.0;
    int    iterations      = 0;

    // -- Routing ---------------------------------------------------------

    /// Each inner vector is a route: an ordered sequence of client ids.
    std::vector<std::vector<int>> routes_;
    /// Clients that could not be served (optional / infeasible).
    std::vector<int> unserved_;

    [[nodiscard]] auto const& routes()   const noexcept { return routes_; }
    [[nodiscard]] auto const& unserved() const noexcept { return unserved_; }

    // -- Scheduling ------------------------------------------------------

    /// Per-operation: (machine, start_time).
    struct OpSchedule {
        int machine    = -1;
        int start_time = 0;
    };
    std::vector<OpSchedule> schedule_;
    int makespan_ = 0;

    [[nodiscard]] auto const& schedule() const noexcept { return schedule_; }
    [[nodiscard]] int makespan()         const noexcept { return makespan_; }

    // -- Assignment (nurse rostering) ------------------------------------

    struct Assignment {
        int         employee = -1;
        int         shift    = -1;
        std::string employee_name;
        std::string shift_name;
    };
    /// Indexed by day: assignments_[day] is the list of assignments.
    std::vector<std::vector<Assignment>> assignments_;
    std::vector<int> unassigned_;

    [[nodiscard]] auto const& assignments() const noexcept { return assignments_; }
    [[nodiscard]] auto const& unassigned()  const noexcept { return unassigned_; }
    /// Convenience: assignments for a given day.
    [[nodiscard]] auto const& day(int d)    const { return assignments_.at(d); }

    // -- Packing ---------------------------------------------------------

    /// Each inner vector is a bin: the item ids placed in that bin.
    std::vector<std::vector<int>> bins_;

    [[nodiscard]] auto const& bins()     const noexcept { return bins_; }
    [[nodiscard]] int         num_bins() const noexcept { return static_cast<int>(bins_.size()); }

    // -- Network / Flow --------------------------------------------------

    /// Per-commodity: list of (path, flow) pairs.
    struct PathFlow {
        std::vector<int> path;
        double           flow = 0.0;
    };
    /// flows_[commodity] = vector of PathFlow.
    std::vector<std::vector<PathFlow>> flows_;

    [[nodiscard]] auto const& flows() const noexcept { return flows_; }
};

} // namespace coso
