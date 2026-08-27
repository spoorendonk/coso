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
//  PSPLIB RCPSP (.sm)
// ---------------------------------------------------------------------------

ScheduleData parse_psplib(const std::string& content) {
    std::istringstream in(content);
    std::string line;

    int num_jobs = 0;  // includes dummy source (1) and sink (N)
    int num_resources = 0;

    // Phase 1: Parse header to find jobs and renewable resources.
    while (std::getline(in, line)) {
        // Look for "jobs (incl. supersource/sink )" line.
        if (line.find("jobs") != std::string::npos &&
            line.find("supersource") != std::string::npos) {
            // Extract the number after the colon.
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                std::istringstream iss(line.substr(colon + 1));
                iss >> num_jobs;
            }
        }
        // "- renewable" resources line.
        if (line.find("renewable") != std::string::npos &&
            line.find("nonrenewable") == std::string::npos &&
            line.find("doubly") == std::string::npos) {
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                std::istringstream iss(line.substr(colon + 1));
                iss >> num_resources;
            }
        }
        // Stop at PRECEDENCE RELATIONS section.
        if (line.find("PRECEDENCE RELATIONS") != std::string::npos) {
            break;
        }
    }

    if (num_jobs <= 0) {
        throw std::runtime_error("PSPLIB parse error: could not find number of jobs");
    }

    // Phase 2: Parse precedence relations.
    // Skip the table header line (jobnr.  #modes  #successors  successors).
    std::getline(in, line);  // header

    // successor_list[j] = list of successor job ids (1-based in file).
    std::vector<std::vector<int>> successor_list(num_jobs + 1);

    for (int j = 0; j < num_jobs; ++j) {
        if (!std::getline(in, line)) {
            throw std::runtime_error("PSPLIB parse error: truncated precedence section");
        }

        // Skip separator lines.
        if (line.find("***") != std::string::npos) {
            --j;
            continue;
        }

        std::istringstream iss(line);
        int job_id = 0, num_modes = 0, num_succ = 0;
        if (!(iss >> job_id >> num_modes >> num_succ)) {
            throw std::runtime_error("PSPLIB parse error: bad precedence line");
        }

        for (int s = 0; s < num_succ; ++s) {
            int succ = 0;
            iss >> succ;
            successor_list[job_id].push_back(succ);
        }
    }

    // Phase 3: Parse REQUESTS/DURATIONS section.
    // Advance to it.
    while (std::getline(in, line)) {
        if (line.find("REQUESTS/DURATIONS") != std::string::npos) {
            break;
        }
    }
    // Skip two header lines.
    std::getline(in, line);  // "jobnr. mode duration R1 R2 ..."
    std::getline(in, line);  // separator "---..."

    struct PspJob {
        int duration = 0;
        std::vector<int> resource_usage;  // per renewable resource
    };
    std::vector<PspJob> psp_jobs(num_jobs + 1);  // 1-indexed

    for (int j = 0; j < num_jobs; ++j) {
        if (!std::getline(in, line)) {
            throw std::runtime_error("PSPLIB parse error: truncated duration section");
        }
        if (line.find("***") != std::string::npos) {
            --j;
            continue;
        }

        std::istringstream iss(line);
        int job_id = 0, mode = 0, dur = 0;
        if (!(iss >> job_id >> mode >> dur)) {
            throw std::runtime_error("PSPLIB parse error: bad duration line");
        }

        psp_jobs[job_id].duration = dur;
        psp_jobs[job_id].resource_usage.resize(num_resources, 0);
        for (int r = 0; r < num_resources; ++r) {
            iss >> psp_jobs[job_id].resource_usage[r];
        }
    }

    // Phase 4: Parse RESOURCEAVAILABILITIES section.
    while (std::getline(in, line)) {
        if (line.find("RESOURCEAVAILABILITIES") != std::string::npos) {
            break;
        }
    }
    std::getline(in, line);  // header "R 1  R 2 ..."

    std::vector<int> capacities(num_resources, 0);
    if (std::getline(in, line)) {
        std::istringstream iss(line);
        for (int r = 0; r < num_resources; ++r) {
            iss >> capacities[r];
        }
    }

    // Phase 5: Build ScheduleData.
    // PSPLIB activities 1..N map to operations. Activities 1 and N are
    // dummy source/sink; we include them as zero-duration operations so
    // precedence arcs referencing them work correctly.
    ScheduleData::Builder builder;

    // RCPSP has no fixed machines -- each activity just needs resources.
    // We still need at least one machine for the framework.  We create
    // a single "virtual" machine that all operations can use.
    builder.add_machine(MachineParams{.name = "virtual"});

    // Add resources.
    std::vector<int> res_ids;
    for (int r = 0; r < num_resources; ++r) {
        res_ids.push_back(builder.add_resource(capacities[r]));
    }

    // Each PSPLIB "job" (activity) becomes one job with one operation.
    // We skip job 0 (unused). Activities are 1..num_jobs.
    std::vector<int> op_index(num_jobs + 1, -1);  // PSPLIB id -> operation index

    for (int a = 1; a <= num_jobs; ++a) {
        int job = builder.add_job();
        OperationParams op;
        op.machine = 0;  // single virtual machine
        op.duration = psp_jobs[a].duration;
        int oidx = builder.add_operation(job, op);
        op_index[a] = oidx;

        // Set resource usage.
        for (int r = 0; r < num_resources; ++r) {
            if (psp_jobs[a].resource_usage[r] > 0) {
                builder.set_resource_usage(oidx, res_ids[r], psp_jobs[a].resource_usage[r]);
            }
        }
    }

    // Add precedence arcs from successor lists.
    for (int a = 1; a <= num_jobs; ++a) {
        for (int succ : successor_list[a]) {
            if (succ >= 1 && succ <= num_jobs) {
                builder.add_precedence(op_index[a], op_index[succ]);
            }
        }
    }

    builder.set_objective(ScheduleObjective::Makespan);
    return builder.build();
}

ScheduleData read_psplib(const std::string& path) {
    return parse_psplib(read_file(path));
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
