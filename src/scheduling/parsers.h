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
//
//  Nothing in the file says how machines are numbered, and both conventions
//  are live: the SchedulingLab/fjsp-instances collection (Brandimarte, Hurink,
//  Dauzere, Barnes, Kacem, Fattahi, Behnke) states "Machine index starts at 0",
//  while the original Brandimarte/Hurink distributions count from 1. The caller
//  says which; a machine outside [0, num_machines) after conversion is
//  rejected, which is how a file read with the wrong convention usually shows.

enum class FjspIndexing {
    ZeroBased,  ///< SchedulingLab/fjsp-instances, the verified v1 set (#202)
    OneBased,   ///< the original Brandimarte/Hurink files
};

/// Parse an FJSP instance from a string.
[[nodiscard]] ScheduleData parse_fjsp(const std::string& content,
                                      FjspIndexing indexing = FjspIndexing::ZeroBased);

/// Read an FJSP instance from a file.
[[nodiscard]] ScheduleData read_fjsp(const std::string& path,
                                     FjspIndexing indexing = FjspIndexing::ZeroBased);

}  // namespace coso
