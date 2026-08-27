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
//  PSPLIB RCPSP format (.sm files)
// ---------------------------------------------------------------------------
//  Header section with project info (jobs, resources).
//  PRECEDENCE RELATIONS section: job_id  num_successors  successor_ids...
//  REQUESTS/DURATIONS section:   job_id  mode  duration  resource_usages...
//  RESOURCEAVAILABILITIES section: capacities per resource.
//
//  Jobs 1 and N are dummy source/sink with duration 0.

/// Parse a PSPLIB RCPSP instance from a string (.sm format).
[[nodiscard]] ScheduleData parse_psplib(const std::string& content);

/// Read a PSPLIB RCPSP instance from a file.
[[nodiscard]] ScheduleData read_psplib(const std::string& path);

// ---------------------------------------------------------------------------
//  Flexible Job Shop (FJSP) standard format
// ---------------------------------------------------------------------------
//  First line:  num_jobs  num_machines  [avg_ops_per_job]
//  Then per job: one line starting with num_operations, followed by
//  per operation: num_eligible_machines, then (machine, duration) pairs.
//  Machine indices are 1-based in the file.

/// Parse an FJSP instance from a string.
[[nodiscard]] ScheduleData parse_fjsp(const std::string& content);

/// Read an FJSP instance from a file.
[[nodiscard]] ScheduleData read_fjsp(const std::string& path);

}  // namespace coso
