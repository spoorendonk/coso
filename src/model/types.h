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
    double work_units = 0.0;  ///< 0.0 = no deterministic work limit
    explicit TimeLimit(double s, double wu = 0.0) : seconds(s), work_units(wu) {}
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
///   - lotsizing:  production(), inventory()
struct Result {
    bool   feasible_        = false;
    double cost_            = 0.0;
    double elapsed_seconds_ = 0.0;
    int    iterations_      = 0;
    uint64_t work_ticks_    = 0;
    double work_units_      = 0.0;

    [[nodiscard]] bool   feasible()        const noexcept { return feasible_; }
    [[nodiscard]] double cost()            const noexcept { return cost_; }
    [[nodiscard]] double elapsed_seconds() const noexcept { return elapsed_seconds_; }
    [[nodiscard]] int    iterations()      const noexcept { return iterations_; }
    [[nodiscard]] uint64_t work_ticks()    const noexcept { return work_ticks_; }
    [[nodiscard]] double work_units()      const noexcept { return work_units_; }

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

    // -- Lot sizing / production -----------------------------------------

    /// production_quantities_[product][period] = produced quantity.
    std::vector<std::vector<double>> production_quantities_;
    /// inventory_levels_[product][period] = end-of-period inventory.
    std::vector<std::vector<double>> inventory_levels_;

    [[nodiscard]] auto const& production() const noexcept {
        return production_quantities_;
    }
    [[nodiscard]] auto const& inventory() const noexcept {
        return inventory_levels_;
    }
};

} // namespace coso
