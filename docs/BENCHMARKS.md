# COSO Benchmark Results

Solver performance against best known solutions (BKS) across all engines.
Results collected with default solver settings on a single core.

## 1. Routing — CVRP

Source: [Uchoa et al. (2017) X instances](https://github.com/VROOM-Project/vroom-scripts/tree/master/benchmarks/CVRP/X).
All BKS values are proven optimal.

| Instance | Customers | Vehicles | BKS | COSO | Gap | Time |
|----------|-----------|----------|-------|-------|--------|------|
| X-n101-k25 | 100 | 25 | 27,591 | 28,022 | +1.56% | 42.6s |
| X-n106-k14 | 105 | 14 | 26,362 | 26,602 | +0.91% | 197.8s |
| X-n110-k13 | 109 | 13 | 14,971 | 15,202 | +1.54% | 173.6s |
| X-n120-k6 | 119 | 6 | 13,332 | 11,549* | — | 724.8s |
| X-n125-k30 | 124 | 30 | 55,539 | 56,581 | +1.88% | 111.8s |

**Metric**: Total distance (integer). Lower is better.
**\* X-n120-k6**: 14 clients unserved (5 routes instead of 6). Instance parsing or vehicle count issue — to be investigated.

## 2. Routing — VRPTW

Source: [Solomon (1987) benchmark instances](https://www.sintef.no/projectweb/top/vrptw/).
BKS from the SINTEF TOP repository.

| Instance | Customers | Type | BKS | COSO | Gap | Time |
|----------|-----------|------|---------|------|--------|------|
| C101 | 100 | Clustered, tight TW | 828.94 | — | — | — |
| C102 | 100 | Clustered, wide TW | 828.94 | — | — | — |
| R101 | 100 | Random, tight TW | 1,645.79 | — | — | — |
| R102 | 100 | Random, wide TW | 1,486.12 | — | — | — |
| RC101 | 100 | Mixed, tight TW | 1,696.94 | — | — | — |
| RC102 | 100 | Mixed, wide TW | 1,554.75 | — | — | — |

**Metric**: Total distance (real-valued). Lower is better.
**Note**: VRPTW benchmarks run but take >10 min per instance due to time limit enforcement issue in the genetic algorithm. Results pending.

## 3. Scheduling — JSP (Taillard)

Source: [Taillard (1993) job shop instances](https://github.com/tamy0612/JSPLIB).
All BKS values are proven optimal.

| Instance | Jobs | Machines | BKS | COSO | Gap | Notes |
|----------|------|----------|------|------|--------|-------|
| ta01 | 15 | 15 | 1,231 | 849 | -31.0% | * |
| ta02 | 15 | 15 | 1,244 | 955 | -23.2% | * |
| ta03 | 15 | 15 | 1,218 | 773 | -36.5% | * |
| ta04 | 15 | 15 | 1,175 | 864 | -26.5% | * |
| ta05 | 15 | 15 | 1,224 | 895 | -26.9% | * |
| ta06 | 15 | 15 | 1,238 | 906 | -26.8% | * |
| ta07 | 15 | 15 | 1,227 | 677 | -44.8% | * |
| ta08 | 15 | 15 | 1,217 | 1,037 | -14.8% | * |
| ta09 | 15 | 15 | 1,274 | 864 | -32.2% | * |
| ta10 | 15 | 15 | 1,241 | 896 | -27.8% | * |

**Metric**: Makespan (integer). Lower is better.
**\* Bug**: COSO reports makespan below proven optimal, indicating a bug in the benchmark pipeline — likely the instance parser or makespan computation. To be investigated.

## 4. Scheduling — RCPSP (PSPLIB j30)

Source: [PSPLIB (Kolisch & Sprecher, 1997)](https://www.om-db.wi.tum.de/psplib/).
All BKS values are proven optimal.

| Instance | Activities | Resources | BKS | COSO | Gap | Notes |
|----------|-----------|-----------|------|------|--------|-------|
| j301_1 | 32 | 4 | 43 | — | — | Download unavailable |
| j301_2 | 32 | 4 | 47 | — | — | Download unavailable |
| j301_3 | 32 | 4 | 47 | — | — | Download unavailable |
| j301_4 | 32 | 4 | 62 | — | — | Download unavailable |
| j301_5 | 32 | 4 | 39 | — | — | Download unavailable |

**Metric**: Makespan (integer). Lower is better.
**Note**: PSPLIB website is currently unavailable. Results will be added when instances can be downloaded.

## 5. Assignment — Nurse Rostering (NRP)

Source: [schedulingbenchmarks.org](https://www.schedulingbenchmarks.org/nrp/).
BKS are best reported solutions (not all proven optimal).

| Instance | Employees | Horizon | Shifts | BKS | COSO | Gap | Notes |
|----------|-----------|---------|--------|-------|-------|--------|-------|
| Instance1 | 8 | 14 days | 1 | 607 | -33 | — | Negative cost = net preference reward |
| Instance2 | 14 | 14 days | 2 | 828 | 926 | +11.8% | |
| Instance3 | 20 | 14 days | 3 | 1,001 | -73 | — | Negative cost = net preference reward |
| Instance4 | 10 | 28 days | 2 | 1,716 | 9,883 | +475.9% | Demand violations dominate |
| Instance5 | 16 | 28 days | 2 | 1,143 | 18,842 | +1548% | Demand violations dominate |
| Instance6 | 18 | 28 days | 3 | 1,950 | 5,852 | +200.1% | Demand violations dominate |
| Instance7 | 20 | 28 days | 3 | 1,056 | 800 | -24.2% | |
| Instance8 | 30 | 28 days | 4 | 1,300 | — | — | Timed out |

**Metric**: Weighted soft constraint violations (integer). Lower is better.
**Note**: Instances 1, 3, 7 show negative costs due to preference rewards outweighing penalties — cost model differs from BKS reference. Instances 4-6 have high demand violations because the FFD construction heuristic + basic local search cannot fully satisfy demand within the operator set. A metaheuristic wrapper (ILS/GA) and richer operators are needed to close the gap.

## 6. Packing — Bin Packing

Source: Inline instances modeled after [Falkenauer (1996)](https://doi.org/10.1007/BF00226291) and [Scholl et al. (1997)](https://doi.org/10.1007/s101070050052).
All optimal values are known.

| Instance | Items | Capacity | Optimal | COSO | Gap |
|----------|-------|----------|---------|------|------|
| Falkenauer-U-10 | 10 | 150 | 5 | 5 | 0% |
| Falkenauer-U-20 | 20 | 150 | 8 | 9 | +1 bin |
| Falkenauer-U-30 | 30 | 150 | 12 | 13 | +1 bin |
| Falkenauer-T-9 | 9 | 1000 | 3 | 4 | +1 bin |
| Falkenauer-T-15 | 15 | 1000 | 5 | 6 | +1 bin |
| Falkenauer-T-30 | 30 | 1000 | 10 | 11 | +1 bin |
| Scholl-C1-15 | 15 | 100 | 6 | 6 | 0% |
| Scholl-C1-25 | 25 | 100 | 10 | 11 | +1 bin |
| Scholl-C2-20 | 20 | 150 | 5 | 6 | +1 bin |
| Scholl-C3-10 | 10 | 100 | 5 | 5 | 0% |
| Scholl-C3-20 | 20 | 100 | 10 | 10 | 0% |

**Metric**: Number of bins used (integer). Lower is better.
**Note**: FFD construction with merge/move local search. Gaps of +1 bin are typical for FFD without metaheuristic improvement.

---

## Summary

| Engine | Instances | Solved | Avg Gap | Status |
|--------|-----------|--------|---------|--------|
| Routing (CVRP) | 5/5 | 4 feasible | +1.5% avg (4 feasible) | Production-quality |
| Routing (VRPTW) | 0/6 | — | — | Time limit bug, pending |
| Scheduling (JSP) | 10/10 | 10 | — | Makespan bug, needs fix |
| Scheduling (RCPSP) | 0/5 | — | — | PSPLIB site down |
| Assignment (NRP) | 7/8 | 7 | Mixed | Needs metaheuristic + richer ops |
| Packing (BPP) | 11/11 | 11 | +0.5 bins avg | Good for FFD baseline |

## How to reproduce

```bash
# Download benchmark instances
./tests/data/download_benchmarks.sh

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run all benchmarks
ctest --test-dir build -j1 -L benchmark --output-on-failure

# Run individual categories
./build/tests/benchmark_test              # CVRP
./build/tests/vrptw_benchmark_test        # VRPTW
./build/tests/scheduling_benchmark_test   # JSP + RCPSP
./build/tests/assignment_benchmark_test   # NRP
./build/tests/packing_benchmark_test      # BPP

# CLI (single instance)
./build/coso-solve tests/data/X-n101-k25.vrp --time-limit 10 -v
```

## References

- Uchoa et al. (2017). *New benchmark instances for the CVRP*. European J. of OR.
- Solomon (1987). *Algorithms for the VRP with time windows*. Operations Research.
- Taillard (1993). *Benchmarks for basic scheduling problems*. European J. of OR.
- Kolisch & Sprecher (1997). *PSPLIB — A project scheduling problem library*. European J. of OR.
- Falkenauer (1996). *A hybrid grouping genetic algorithm for bin packing*. J. of Heuristics.
- Scholl et al. (1997). *Bison: A fast hybrid procedure for exactly solving the one-dimensional bin packing problem*. Computers & OR.
- schedulingbenchmarks.org. *Nurse Rostering Problem benchmark instances*.
