#include "model/routing_model.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

namespace {

struct Options {
    std::string instance_path;
    double time_limit = 30.0;
    double work_limit = 0.0;
    bool verbose = false;
};

void print_usage(const char* prog)
{
    std::cerr << "Usage: " << prog
              << " <instance.vrp> [--time-limit <seconds>] [-v|--verbose]\n"
              << "\n"
              << "Solve a CVRPLIB .vrp instance using COSO.\n"
              << "\n"
              << "Positional arguments:\n"
              << "  instance.vrp          Path to a CVRPLIB .vrp file\n"
              << "\n"
              << "Options:\n"
              << "  --time-limit <sec>    Solver time limit in seconds (default: 30)\n"
              << "  --work-limit <units>  Deterministic work limit (default: 0 = off)\n"
              << "  -v, --verbose         Print route details\n"
              << "  -h, --help            Show this help message\n";
}

/// Parse command-line arguments. Returns true on success.
bool parse_args(int argc, char* argv[], Options& opts)
{
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            std::exit(0);
        }

        if (arg == "--time-limit") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --time-limit requires a value\n";
                return false;
            }
            ++i;
            try {
                opts.time_limit = std::stod(argv[i]);
            } catch (...) {
                std::cerr << "Error: invalid time limit: " << argv[i] << "\n";
                return false;
            }
            if (opts.time_limit <= 0.0) {
                std::cerr << "Error: time limit must be positive\n";
                return false;
            }
            continue;
        }

        if (arg == "--work-limit") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --work-limit requires a value\n";
                return false;
            }
            ++i;
            try {
                opts.work_limit = std::stod(argv[i]);
            } catch (...) {
                std::cerr << "Error: invalid work limit: " << argv[i] << "\n";
                return false;
            }
            if (opts.work_limit < 0.0) {
                std::cerr << "Error: work limit must be non-negative\n";
                return false;
            }
            continue;
        }

        if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
            continue;
        }

        // Unknown option (starts with '-')
        if (arg.starts_with('-')) {
            std::cerr << "Error: unknown option: " << arg << "\n";
            return false;
        }

        // Positional argument: instance path
        if (opts.instance_path.empty()) {
            opts.instance_path = std::string(arg);
        } else {
            std::cerr << "Error: unexpected argument: " << arg << "\n";
            return false;
        }
    }

    if (opts.instance_path.empty()) {
        std::cerr << "Error: no instance file specified\n";
        return false;
    }

    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    Options opts;
    if (!parse_args(argc, argv, opts)) {
        std::cerr << "\n";
        print_usage(argv[0]);
        return 1;
    }

    std::cout << "Instance: " << opts.instance_path << "\n";
    std::cout << "Time limit: " << opts.time_limit << "s\n";
    if (opts.work_limit > 0.0) {
        std::cout << "Work limit: " << opts.work_limit << "\n";
    }
    std::cout << "Solving...\n" << std::flush;

    coso::Result result;
    try {
        result = coso::solve(opts.instance_path,
                             coso::TimeLimit(opts.time_limit, opts.work_limit));
    } catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // Print summary.
    std::cout << "\n";
    std::cout << "Feasible: " << (result.feasible() ? "yes" : "no") << "\n";
    std::cout << "Cost:     " << std::fixed << std::setprecision(0)
              << result.cost() << "\n";
    std::cout << "Routes:   " << result.routes().size() << "\n";
    std::cout << "Elapsed:  " << std::fixed << std::setprecision(2)
              << result.elapsed_seconds() << "s\n";
    std::cout << "Iters:    " << result.iterations() << "\n";
    std::cout << "Work:     " << std::fixed << std::setprecision(2)
              << result.work_units() << " (" << result.work_ticks()
              << " ticks)\n";

    if (!result.unserved().empty()) {
        std::cout << "Unserved: " << result.unserved().size() << " clients\n";
    }

    // Verbose: print each route.
    if (opts.verbose) {
        std::cout << "\nRoutes:\n";
        int idx = 0;
        for (auto const& route : result.routes()) {
            std::cout << "  Route " << idx++ << ": ";
            for (size_t i = 0; i < route.size(); ++i) {
                if (i > 0) std::cout << " -> ";
                std::cout << route[i];
            }
            std::cout << "\n";
        }
    }

    return result.feasible() ? 0 : 2;
}
