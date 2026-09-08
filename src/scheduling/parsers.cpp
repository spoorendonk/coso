#include "scheduling/parsers.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace coso {

namespace {

/// Skip blank lines and lines starting with '#'.
/// Returns the next non-blank, non-comment line, or empty string on EOF.
std::string next_data_line(std::istream& in) {
    std::string line;
    while (std::getline(in, line)) {
        // Trim leading whitespace.
        auto pos = line.find_first_not_of(" \t\r\n");
        if (pos == std::string::npos) {
            continue;  // blank
        }
        if (line[pos] == '#') {
            continue;  // comment
        }
        return line;
    }
    return {};
}

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
//  Taillard JSP
// ---------------------------------------------------------------------------

ScheduleData parse_taillard_jsp(const std::string& content) {
    std::istringstream in(content);

    // First data line: num_jobs  num_machines
    std::string header = next_data_line(in);
    if (header.empty()) {
        throw std::runtime_error("Taillard JSP parse error: empty input");
    }

    int num_jobs = 0, num_machines = 0;
    {
        std::istringstream iss(header);
        if (!(iss >> num_jobs >> num_machines) || num_jobs <= 0 || num_machines <= 0) {
            throw std::runtime_error("Taillard JSP parse error: invalid header");
        }
    }

    ScheduleData::Builder builder;

    // Add machines.
    for (int m = 0; m < num_machines; ++m) {
        builder.add_machine();
    }

    // Read jobs: each job has num_machines operations as (machine, duration) pairs.
    for (int j = 0; j < num_jobs; ++j) {
        std::string line = next_data_line(in);
        if (line.empty()) {
            throw std::runtime_error("Taillard JSP parse error: expected job " + std::to_string(j));
        }

        int job = builder.add_job();
        std::istringstream iss(line);

        for (int o = 0; o < num_machines; ++o) {
            int machine = 0, duration = 0;
            if (!(iss >> machine >> duration)) {
                throw std::runtime_error("Taillard JSP parse error: job " + std::to_string(j) +
                                         ", operation " + std::to_string(o));
            }

            OperationParams params;
            params.machine = machine;
            params.duration = duration;
            builder.add_operation(job, params);
        }
    }

    builder.set_objective(ScheduleObjective::Makespan);
    return builder.build();
}

ScheduleData read_taillard_jsp(const std::string& path) {
    return parse_taillard_jsp(read_file(path));
}

// ---------------------------------------------------------------------------
//  Flexible Job Shop (FJSP)
// ---------------------------------------------------------------------------

ScheduleData parse_fjsp(const std::string& content) {
    std::istringstream in(content);

    // First data line: num_jobs  num_machines  [avg_ops_per_job]
    std::string header = next_data_line(in);
    if (header.empty()) {
        throw std::runtime_error("FJSP parse error: empty input");
    }

    int num_jobs = 0, num_machines = 0;
    {
        std::istringstream iss(header);
        if (!(iss >> num_jobs >> num_machines) || num_jobs <= 0 || num_machines <= 0) {
            throw std::runtime_error("FJSP parse error: invalid header");
        }
        // Optional third value (avg_ops_per_job) is ignored.
    }

    ScheduleData::Builder builder;

    for (int m = 0; m < num_machines; ++m) {
        builder.add_machine();
    }

    for (int j = 0; j < num_jobs; ++j) {
        std::string line = next_data_line(in);
        if (line.empty()) {
            throw std::runtime_error("FJSP parse error: expected job " + std::to_string(j));
        }

        int job = builder.add_job();
        std::istringstream iss(line);

        int num_ops = 0;
        if (!(iss >> num_ops) || num_ops <= 0) {
            throw std::runtime_error("FJSP parse error: invalid num_ops for job " +
                                     std::to_string(j));
        }

        for (int o = 0; o < num_ops; ++o) {
            int num_eligible = 0;
            if (!(iss >> num_eligible) || num_eligible <= 0) {
                throw std::runtime_error("FJSP parse error: job " + std::to_string(j) + ", op " +
                                         std::to_string(o) + ": bad num_eligible");
            }

            OperationParams params;

            if (num_eligible == 1) {
                // Single eligible machine -- use fixed machine assignment.
                int machine = 0, duration = 0;
                if (!(iss >> machine >> duration)) {
                    throw std::runtime_error("FJSP parse error: job " + std::to_string(j) +
                                             ", op " + std::to_string(o));
                }
                params.machine = machine - 1;  // 1-based -> 0-based
                params.duration = duration;
            } else {
                // Multiple eligible machines.
                params.eligible_machines.reserve(num_eligible);
                params.durations_per_machine.reserve(num_eligible);
                for (int e = 0; e < num_eligible; ++e) {
                    int machine = 0, duration = 0;
                    if (!(iss >> machine >> duration)) {
                        throw std::runtime_error("FJSP parse error: job " + std::to_string(j) +
                                                 ", op " + std::to_string(o) + ", eligible " +
                                                 std::to_string(e));
                    }
                    params.eligible_machines.push_back(machine - 1);
                    params.durations_per_machine.push_back(duration);
                }
            }

            builder.add_operation(job, params);
        }
    }

    builder.set_objective(ScheduleObjective::Makespan);
    return builder.build();
}

ScheduleData read_fjsp(const std::string& path) {
    return parse_fjsp(read_file(path));
}

}  // namespace coso
