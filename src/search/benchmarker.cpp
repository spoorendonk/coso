#include "search/benchmarker.h"

#include "model/routing_model.h"
#include "model/instance_reader.h"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace coso {

Benchmarker::Benchmarker(double time_limit_s) : time_limit_s_(time_limit_s) {}

BenchmarkResult Benchmarker::run_instance(std::string const& path, int bks)
{
    BenchmarkResult res;
    res.instance = fs::path(path).filename().string();
    res.bks = bks;

    auto result = coso::solve(path, TimeLimit(time_limit_s_));

    res.cost = static_cast<int64_t>(result.cost());
    res.elapsed_s = result.elapsed_seconds();
    res.work_units = result.work_units();
    res.work_ticks = result.work_ticks();
    res.feasible = result.feasible();
    res.num_routes = static_cast<int>(result.routes().size());

    if (bks > 0 && res.feasible && res.cost > 0) {
        res.gap_pct = (static_cast<double>(res.cost) - bks) / bks * 100.0;
    }

    return res;
}

std::vector<BenchmarkResult>
Benchmarker::run_directory(std::string const& dir)
{
    std::vector<BenchmarkResult> results;

    if (!fs::is_directory(dir)) {
        return results;
    }

    // Collect and sort .vrp files for deterministic ordering.
    std::vector<fs::path> paths;
    for (auto const& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".vrp") {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());

    for (auto const& p : paths) {
        int bks = lookup_bks(p.filename().string());
        results.push_back(run_instance(p.string(), bks));
    }

    return results;
}

std::string
Benchmarker::to_csv(std::vector<BenchmarkResult> const& results)
{
    std::ostringstream ss;
    ss << std::fixed;

    ss << "instance,bks,cost,gap_pct,elapsed_s,work_units,work_ticks,feasible,num_routes\n";

    for (auto const& r : results) {
        ss << r.instance << ","
           << r.bks << ","
           << r.cost << ","
           << std::setprecision(2) << r.gap_pct << ","
           << std::setprecision(3) << r.elapsed_s << ","
           << std::setprecision(3) << r.work_units << ","
           << r.work_ticks << ","
           << (r.feasible ? "true" : "false") << ","
           << r.num_routes << "\n";
    }

    return ss.str();
}

void Benchmarker::print_summary(std::vector<BenchmarkResult> const& results)
{
    if (results.empty()) {
        std::cout << "No benchmark results.\n";
        return;
    }

    int total = static_cast<int>(results.size());
    int feasible_count = 0;
    int with_bks = 0;
    double sum_gap = 0.0;
    double max_gap = 0.0;

    for (auto const& r : results) {
        if (r.feasible) ++feasible_count;
        if (r.bks > 0 && r.feasible && r.cost > 0) {
            ++with_bks;
            sum_gap += r.gap_pct;
            max_gap = std::max(max_gap, r.gap_pct);
        }
    }

    double avg_gap = (with_bks > 0) ? sum_gap / with_bks : 0.0;

    std::cout << "\n=== Benchmark Summary ===\n"
              << "  Instances:    " << total << "\n"
              << "  Feasible:     " << feasible_count << " / " << total << "\n"
              << "  With BKS:     " << with_bks << "\n"
              << std::fixed
              << "  Avg gap:      " << std::setprecision(2) << avg_gap << "%\n"
              << "  Max gap:      " << std::setprecision(2) << max_gap << "%\n"
              << std::endl;
}

int Benchmarker::lookup_bks(std::string const& filename)
{
    auto const& table = bks_table();
    auto it = table.find(filename);
    return (it != table.end()) ? it->second : 0;
}

std::unordered_map<std::string, int> const& Benchmarker::bks_table()
{
    // BKS values for Uchoa et al. X-set instances.
    // Source: CVRPLIB (http://vrp.atd-lab.inf.puc-rio.br/)
    static std::unordered_map<std::string, int> const table = {
        {"X-n101-k25.vrp", 27591},
        {"X-n106-k14.vrp", 26362},
        {"X-n110-k13.vrp", 14971},
        {"X-n115-k10.vrp", 12747},
        {"X-n120-k6.vrp",  6942},
        {"X-n125-k30.vrp", 55539},
        {"X-n129-k18.vrp", 28940},
        {"X-n134-k13.vrp", 10916},
        {"X-n139-k10.vrp", 13590},
        {"X-n143-k7.vrp",  15700},
        {"X-n148-k46.vrp", 43448},
        {"X-n153-k22.vrp", 21220},
        {"X-n157-k13.vrp", 16876},
        {"X-n162-k11.vrp", 14138},
        {"X-n167-k10.vrp", 20557},
        {"X-n172-k51.vrp", 45607},
        {"X-n176-k26.vrp", 47812},
        {"X-n181-k23.vrp", 25569},
        {"X-n186-k15.vrp", 24145},
        {"X-n190-k8.vrp",  16980},
        {"X-n195-k51.vrp", 44225},
        {"X-n200-k36.vrp", 58578},
        {"X-n204-k19.vrp", 19565},
        {"X-n209-k16.vrp", 30656},
        {"X-n214-k11.vrp", 10856},
        {"X-n219-k73.vrp", 117595},
        {"X-n223-k34.vrp", 40437},
        {"X-n228-k23.vrp", 25742},
        {"X-n233-k16.vrp", 19230},
        {"X-n237-k14.vrp", 27042},
        {"X-n242-k48.vrp", 82751},
        {"X-n247-k50.vrp", 37274},
        {"X-n251-k28.vrp", 38684},
        {"X-n256-k16.vrp", 18839},
        {"X-n261-k13.vrp", 26558},
        {"X-n266-k58.vrp", 75478},
        {"X-n270-k35.vrp", 35291},
        {"X-n275-k28.vrp", 21245},
        {"X-n280-k17.vrp", 33503},
        {"X-n284-k15.vrp", 20215},
        {"X-n289-k60.vrp", 95151},
        {"X-n294-k50.vrp", 47161},
        {"X-n298-k31.vrp", 34231},
        {"X-n303-k21.vrp", 21736},
        {"X-n308-k13.vrp", 25859},
        {"X-n313-k71.vrp", 94043},
        {"X-n317-k53.vrp", 78355},
        {"X-n322-k28.vrp", 29834},
        {"X-n327-k20.vrp", 27532},
        {"X-n331-k15.vrp", 31102},
        {"X-n336-k84.vrp", 139111},
        {"X-n344-k43.vrp", 42050},
        {"X-n351-k40.vrp", 25896},
        {"X-n359-k29.vrp", 51505},
        {"X-n367-k17.vrp", 22814},
        {"X-n376-k94.vrp", 147713},
        {"X-n384-k52.vrp", 65940},
        {"X-n393-k38.vrp", 38260},
        {"X-n401-k29.vrp", 66154},
        {"X-n411-k19.vrp", 19712},
        {"X-n420-k130.vrp", 107798},
        {"X-n429-k61.vrp", 65449},
        {"X-n439-k37.vrp", 36391},
        {"X-n449-k29.vrp", 55233},
        {"X-n459-k26.vrp", 24139},
        {"X-n469-k138.vrp", 221824},
        {"X-n480-k70.vrp", 89449},
        {"X-n491-k59.vrp", 66483},
        {"X-n502-k39.vrp", 69226},
        {"X-n513-k21.vrp", 24201},
        {"X-n524-k153.vrp", 154593},
        {"X-n536-k96.vrp", 94846},
        {"X-n548-k50.vrp", 86700},
        {"X-n561-k42.vrp", 42717},
        {"X-n573-k30.vrp", 50673},
        {"X-n586-k159.vrp", 190316},
        {"X-n599-k92.vrp", 108451},
        {"X-n613-k62.vrp", 59535},
        {"X-n627-k43.vrp", 62164},
        {"X-n641-k35.vrp", 63682},
        {"X-n655-k131.vrp", 106780},
        {"X-n670-k130.vrp", 146332},
        {"X-n685-k75.vrp", 68205},
        {"X-n701-k44.vrp", 81923},
        {"X-n716-k35.vrp", 43373},
        {"X-n733-k159.vrp", 136187},
        {"X-n749-k98.vrp", 77269},
        {"X-n766-k71.vrp", 114454},
        {"X-n783-k48.vrp", 72386},
        {"X-n801-k40.vrp", 73305},
        {"X-n819-k171.vrp", 158121},
        {"X-n837-k142.vrp", 193737},
        {"X-n856-k95.vrp", 88965},
        {"X-n876-k59.vrp", 99299},
        {"X-n895-k37.vrp", 53860},
        {"X-n916-k207.vrp", 329179},
        {"X-n936-k151.vrp", 132715},
        {"X-n957-k87.vrp", 85465},
        {"X-n979-k58.vrp", 118976},
        {"X-n1001-k43.vrp", 72355},
    };
    return table;
}

} // namespace coso
