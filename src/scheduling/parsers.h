#pragma once

#include "scheduling/schedule_data.h"

#include <string>

namespace coso {

// ---------------------------------------------------------------------------
//  Taillard JSP format
// ---------------------------------------------------------------------------
//  First line:  num_jobs  num_machines
//  Then num_jobs lines, each with num_machines pairs: (machine, duration)
//
//  Operations are implicitly ordered within each job (first pair = first op).

/// Parse a Taillard JSP instance from a string.
[[nodiscard]] ScheduleData parse_taillard_jsp(const std::string& content);

/// Read a Taillard JSP instance from a file.
[[nodiscard]] ScheduleData read_taillard_jsp(const std::string& path);

// ---------------------------------------------------------------------------
//  Flexible Job Shop (FJSP) standard format
// ---------------------------------------------------------------------------
//  First line:  num_jobs  num_machines  [avg_machines_per_operation]
//  Then per job: one line starting with num_operations, followed by
//  per operation: num_eligible_machines, then (machine, duration) pairs.
//  Machine indices are 1-based in the file.

/// Parse an FJSP instance from a string.
[[nodiscard]] ScheduleData parse_fjsp(const std::string& content);

/// Read an FJSP instance from a file.
[[nodiscard]] ScheduleData read_fjsp(const std::string& path);

}  // namespace coso
