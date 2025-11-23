# Flowty LP-Free Heuristic Solver — Architecture & Implementation Plan

This file summarizes the design we discussed and lays out a concrete implementation plan for a C++ library that:

- Has a **MIP-like modeling interface**.
- Uses a **JuLS-style constraint/LS core**.
- Includes an **FJ-style (Feasibility Jump) core for the linear block**.
- Exposes a **ROAR-style algorithm interface** so meta-heuristics are cleanly pluggable.

The plan is written so a code-generation model (Codex, Claude, etc.) can implement it step by step.

---

## 1. High-Level Architecture

Three main layers:

1. **Modeling API (user-facing)**
   - MIP-like DSL: variables, linear constraints, objective.
   - Extended with Flowty-specific **graph and subproblem** constructs (e.g., path/route subproblem).

2. **Core IR + Engines (internal)**
   - **IR (Intermediate Representation)**: canonical in-memory model:
     - Variables, linear constraints, objective, graph/subproblems, current solution state.
   - **JuLS-style core**:
     - Constraint/variable engine, incremental evaluation, neighborhoods for local search.
   - **FJ core**:
     - Lagrangian/penalty-based feasibility search on the linear block (LP-free).

3. **ROAR-style Algorithm Layer (external interface for search)**
   - Abstract `Problem` / `Solution` interface.
   - Algorithms (LS, SA, LNS, FJ, hybrid FJ+JuLS) implemented on that interface.
   - Portfolio / controller to schedule algorithms.

At any given time, **one algorithm** drives the search. It calls into the cores (JuLS, FJ) as services.

---

## 2. Modeling Layer (User-Facing API)

### 2.1 Core concepts

Expose a C++ API roughly like:

```cpp
class Model {
public:
    // Variables
    BoolVar addBoolVar(std::string name);
    IntVar  addIntVar(std::string name, int lb, int ub);
    DoubleVar addContinuousVar(std::string name, double lb, double ub);

    // Algebraic expressions
    LinExpr linear(); // or operator overloading with Var

    // Constraints
    Constraint addConstraint(const LinExpr& expr, Sense s, double rhs, std::string name = "");

    // Objective
    void minimize(const LinExpr& expr);
    void maximize(const LinExpr& expr);

    // Flowty graph/subproblem
    Graph addGraph(std::string name, int num_nodes);
    PathSubproblem addPathSubproblem(std::string name, const Graph& g);

    // Solve
    SolveResult solve(const SolveOptions& opts);
};
```

For now, **graph/subproblem** is just stubbed; the initial focus is generic MIP. The API should be designed so graph constructs can be added cleanly later.

### 2.2 Mapping to IR

When `solve()` is called:

- The model builds an **IR** object:

```cpp
struct VarIR {
    int id;
    enum Type { BOOL, INT, CONT } type;
    double lb, ub;
};

struct LinConIR {
    std::vector<int> var_idx;
    std::vector<double> coeff;
    double rhs;
    enum Sense { GE, LE, EQ } sense;
};

struct ObjectiveIR {
    std::vector<int> var_idx;
    std::vector<double> coeff;
    bool minimize;
};

struct IR {
    std::vector<VarIR> vars;
    std::vector<LinConIR> cons;
    ObjectiveIR objective;

    // later: graph IR, subproblem IR, etc.
};
```

This IR is what the cores operate on.

---

## 3. State & IR at Runtime

### 3.1 State representation

The **State** holds a candidate solution and derived info:

```cpp
struct State {
    std::vector<int>    int_vals;   // integer / bool vars
    std::vector<double> cont_vals;  // continuous vars (later)

    double objective_value;         // current c^T x
    std::vector<double> cons_viol;  // per-constraint violation
    double total_violation;         // sum or norm of violations

    // later: subproblem/graph-specific state fields
};
```

For a pure MIP prototype you can initially store everything as `int` and treat continuous separately later.

---

## 4. JuLS-Style Core

This is the generic constraint/LS engine:

- **Knows the IR** (variables, constraints, objective).
- Maintains **incremental evaluation**:
  - Change a variable → update objective and constraint violations efficiently.
- Provides **generic neighborhoods**:
  - Single-variable moves, multi-variable moves, diving moves, etc.
- Eventually: global constraints and graph/subproblem hooks.

### 4.1 Engine skeleton

```cpp
class JuLSCore {
public:
    JuLSCore(const IR& ir);

    void recompute(State& s) const;
    void applyDelta(State& s, int var_idx, int int_delta) const;

    // For now, penalty objective:
    double penalizedCost(const State& s, double penalty_weight) const;

private:
    const IR& ir_;
    // optional: precomputed A matrix, row activities, etc.
};
```

### 4.2 Neighborhoods

Start with a simple int-based neighborhood for MIP:

```cpp
struct VarDeltaMove {
    int var_idx;
    int delta;  // e.g., -1, +1
};

class SingleVarNeighborhood {
public:
    void generate(const State& s, std::vector<VarDeltaMove>& moves) const;
};
```

Later you add:

- Variable-specific step sizes.
- Rounding neighborhoods from continuous relaxations (if you choose).
- Structure-based moves for graph/subproblems.

### 4.3 Local search primitive

Implement a reusable **LS step**:

```cpp
class LocalSearchStep {
public:
    LocalSearchStep(const JuLSCore& core, double penalty_weight);

    bool firstImprovement(State& s); // returns true if improved

private:
    const JuLSCore& core_;
    double penalty_weight_;
    SingleVarNeighborhood neigh_;
};
```

This LS primitive is what higher-level algorithms will call.

---

## 5. FJ Core (Feasibility Jump)

The FJ core operates on the **linear block**:

- Uses the same IR and State.
- Maintains **constraint multipliers** (λ).
- Optimizes **weighted violation** (Lagrangian penalty) via variable moves.
- Does **multiplier updates** when stuck.

### 5.1 FJ core skeleton

```cpp
class FJCore {
public:
    FJCore(const IR& ir);

    // Initialize multipliers
    void initMultipliers();

    // One FJ iteration: either a move or a multiplier update
    void step(State& s, const JuLSCore& core);

private:
    const IR& ir_;
    std::vector<double> lambda_; // one per constraint

    double lagrangianPenalty(const State& s) const;
    VarDeltaMove bestPenaltyMove(const State& s, const JuLSCore& core) const;
    void updateMultipliers(const State& s);
};
```

Algorithm sketch:

- `lagrangianPenalty(s) = Σ_i λ_i * viol_i(s)`.
- `bestPenaltyMove` tries candidate var moves (e.g., +-1, or more advanced) and picks the one minimizing penalty.
- If no move reduces penalty, call `updateMultipliers()`:
  - Increase λ for violated constraints.
  - Possibly decrease for satisfied ones.
  - This is where you mimic FJ logic.

Later you can add more advanced FJ behaviour (multi-variable jumps, constraint selection, etc.), but skeleton above is enough to wire the architecture.

---

## 6. ROAR-Style Algorithm Layer

Here you define how algorithms see the problem. Minimal interface:

```cpp
struct SolutionHandle {
    State* state;
};

class Problem {
public:
    virtual ~Problem() = default;

    virtual SolutionHandle initialSolution() = 0;
    virtual double evaluate(const SolutionHandle& h) const = 0;
    virtual bool isFeasible(const SolutionHandle& h) const = 0;
};
```

For this library you'll create a `HybridMipProblem` that:

- Wraps the IR, `JuLSCore`, `FJCore`.
- Creates an initial State.
- Provides accessors for cores (since hybrid algorithms need them).

```cpp
class HybridMipProblem : public Problem {
public:
    HybridMipProblem(const IR& ir, JuLSCore& juls, FJCore& fj);

    SolutionHandle initialSolution() override;
    double evaluate(const SolutionHandle& h) const override;
    bool isFeasible(const SolutionHandle& h) const override;

    JuLSCore& julsCore() { return juls_; }
    FJCore&   fjCore()   { return fj_;  }

private:
    const IR& ir_;
    JuLSCore& juls_;
    FJCore&   fj_;
    double penalty_weight_ = 1000.0; // later configurable
};
```

### 6.1 Algorithms

Define an abstract base:

```cpp
class Algorithm {
public:
    virtual ~Algorithm() = default;
    virtual void run(Problem& problem,
                     double time_limit_seconds,
                     SolutionHandle& best) = 0;
};
```

Examples:

- `FirstImprovementLSAlgorithm` (pure LS via JuLS).
- `FeasibilityJumpAlgorithm` (pure FJ).
- `HybridFJJuLSAlgorithm` (alternate FJ & LS phases).
- Later: LNS, SA, GRASP, etc.

#### Example: Hybrid FJ + JuLS algorithm

```cpp
class HybridFJJuLSAlgorithm : public Algorithm {
public:
    HybridFJJuLSAlgorithm(int fj_iters_per_phase,
                          int ls_iters_per_phase);

    void run(Problem& base_problem,
             double time_limit_seconds,
             SolutionHandle& best) override;

private:
    int fj_iters_;
    int ls_iters_;
};
```

Inside `run`:

- Downcast `Problem&` to `HybridMipProblem&`.
- Get `JuLSCore&` and `FJCore&`.
- Loop until time limit:
  - FJ phase: `fj_core.step(*cur.state, juls_core)` repeated `fj_iters_`.
  - LS phase: `LocalSearchStep(juls_core, penalty_weight).firstImprovement(*cur.state)` repeated `ls_iters_`.
  - Track best feasible solution.

---

## 7. Concrete Mini Examples

### 7.1 General MIP (tiny example)

Model:

```cpp
// User
Model m;
auto x1 = m.addBoolVar("x1");
auto x2 = m.addIntVar("x2", 0, 10);
auto x3 = m.addIntVar("x3", 0, 10);
m.addConstraint(2*x1 + x2 + x3 >= 4, "C1");
m.addConstraint(   x1 + 3*x2 + 2*x3 <= 5, "C2");
m.minimize(3*x1 + 2*x2 + 5*x3);
auto result = m.solve(opts);
```

Inside:

- IR: 3 vars, 2 constraints, 1 objective.
- `JuLSCore` uses IR to compute objective & violations.
- `FJCore` uses IR (same constraints) for penalty search.
- `HybridMipProblem` uses both cores.
- `HybridFJJuLSAlgorithm` runs on that problem and returns a solution.

### 7.2 TSP (later, once graph subproblem support is added)

- Modeling layer:
  - `Graph g = m.addCompleteGraph("cities", n);`
  - `PathSubproblem tour = m.addPathSubproblem("tour", g).asHamiltonianCycle();`
  - `m.minimize(tour.totalCost());`
- IR:
  - Graph IR and a `PathSubproblemIR` with successor representation or similar.
- JuLS core:
  - Successor variables, `AllDifferent`/`Circuit` constraints, incremental evaluation.
  - TSP-specific neighborhoods (2-opt, 3-opt, etc.).
- FJ core:
  - Possibly limited to linking linear constraints (if any).
- Algorithms:
  - LS / SA / LNS using the same ROAR-style patterns.

---

## 8. Implementation Plan for Codegen Models

The goal is to make this piecemeal enough that an LLM can write code reliably. Break it into phases.

### Phase 0 — Repo and Build Skeleton

**Tasks:**

1. Create C++ project layout:
   - `include/flowty/`
   - `src/`
   - `tests/`
2. Set up CMake (or your build system).
3. Decide on dependencies: ideally none beyond STL for v1.

**Prompts for the coder model:**

- *"Create a minimal CMake-based C++ project with library `flowty_core` and a test executable. Use C++20, structure folders as `include/flowty` and `src`."*

### Phase 1 — IR and Basic State

**Tasks:**

1. Implement IR structs:
   - `VarIR`, `LinConIR`, `ObjectiveIR`, `IR`.
2. Implement `State` struct:
   - Holds variable values, objective, per-constraint violations.

3. Implement a small unit-test that:
   - Constructs IR for the tiny 3-var, 2-con MIP.
   - Creates a default `State` with some values.

**Prompts:**

- *"Implement IR classes (`VarIR`, `LinConIR`, `ObjectiveIR`, `IR`) representing a linear MIP model; add a test that instantiates a simple 3-variable model."*
- *"Implement a `State` struct that holds integer values, objective value, constraint violations; write a test that populates it and checks sizes."*

### Phase 2 — JuLSCore (incremental evaluation & neighborhood)

**Tasks:**

1. Implement `JuLSCore`:
   - Constructor taking `IR`.
   - `recompute(State&)` for objective + constraint violations.
   - `applyDelta(State&, int var_idx, int delta)` for small updates (can recompute fully at first; incremental later).

2. Implement `SingleVarNeighborhood` and `VarDeltaMove`.

3. Implement `LocalSearchStep` with a simple first-improvement LS over the neighborhood.

4. Tests:
   - Verify `recompute` gives correct objective and violations for several states.
   - Verify `LocalSearchStep::firstImprovement` either improves penalized cost or returns false if no improving move.

**Prompts:**

- *"Given IR and State, implement a JuLSCore class that can recompute objective and constraint violations. Assume linear constraints only."*
- *"Implement SingleVarNeighborhood to generate +-1 moves per integer variable respecting bounds supplied by VarIR."*
- *"Implement LocalSearchStep::firstImprovement using JuLSCore::penalizedCost = objective + penalty_weight * total_violation."*

### Phase 3 — FJCore (minimal feasibility jump)

**Tasks:**

1. Implement `FJCore`:
   - Stores multipliers `lambda` per constraint.
   - `lagrangianPenalty(State&)`.
   - `bestPenaltyMove(State&, JuLSCore&)`: try +-1 for each var, pick best penalty.

2. Implement `step(State&, JuLSCore&)`:
   - If `bestPenaltyMove` improves penalty, apply it.
   - Else update multipliers (simple scheme: multiply λ_i by factor>1 for violated constraints).

3. Tests:
   - Construct a simple infeasible state; run several `step()` calls and check that:
     - Total violation or Lagrangian penalty decreases over time (monotonic test with some allowance).
     - Multipliers change when stuck.

**Prompts:**

- *"Implement class FJCore that uses IR and State to compute a Lagrangian penalty = sum lambda_i * viol_i. Add a method step that tries +-1 moves on each variable and updates multipliers when no improving move exists."*
- *"Write unit tests that check FJCore reduces violation or penalty on the example 3-variable MIP."*

### Phase 4 — ROAR-Style Problem and Algorithms

**Tasks:**

1. Implement `SolutionHandle` and `Problem` base class.

2. Implement `HybridMipProblem`:
   - Contains `IR`, `JuLSCore`, `FJCore`.
   - Creates an initial solution.
   - Implements `evaluate` (objective + penalty).
   - Implements `isFeasible`.

3. Implement `HybridFJJuLSAlgorithm` using:
   - A loop that runs FJCore steps (feasibility phase).
   - Then LS steps (objective phase).
   - Tracks best feasible.

4. Implement a simple `Solver` class or `Model::solve()` that:
   - Builds IR from the modeling data.
   - Instantiates `JuLSCore`, `FJCore`, `HybridMipProblem`, `HybridFJJuLSAlgorithm`.
   - Returns the best solution.

5. Tests:
   - On the tiny MIP, run `Model::solve()` and assert feasibility and some bound on objective.

**Prompts:**

- *"Implement an abstract Problem class and a concrete HybridMipProblem that wraps IR, JuLSCore, FJCore. HybridMipProblem should provide initialSolution, evaluate, isFeasible."*
- *"Implement HybridFJJuLSAlgorithm that alternates N FJCore steps and M JuLSCore LS steps and keeps the best feasible solution seen."*
- *"Wire Model::solve() so it builds IR, then constructs these cores and algorithm and returns the best solution."*

### Phase 5 — Clean-up, Config, and Extension Points

**Tasks:**

1. Make penalty weights configurable via `SolveOptions`.

2. Add logging / callbacks for:
   - Current best objective.
   - Feasibility status.

3. Add hooks in IR and State to later extend:
   - Graph/subproblem IR types.
   - Global constraint types.

4. Document public API:
   - Modeling API.
   - Supported algorithms and options.

**Prompts:**

- *"Add a SolveOptions struct that lets the user configure time limit and penalty weight; thread it through Model::solve → HybridMipProblem → algorithms."*
- *"Add logging hooks to HybridFJJuLSAlgorithm to report improvements and feasibility changes via callbacks."*

### Phase 6 — Graph/Subproblem Support (later)

Once the core MIP + FJ + LS infrastructure is stable, then:

1. Extend IR with:
   - Graph IR (nodes, arcs, arc costs).
   - Subproblem IR for paths/routes.

2. Extend `State` and `JuLSCore` with subproblem states and constraints (`AllDifferent`, `Circuit`, capacity).

3. Implement neighborhoods for graph subproblems (2-opt, insertion, etc.).

4. Expose modeling API:
   - `Graph`, `PathSubproblem` as discussed.

This will be a large extension, so treat it as a separate design mini-spec.

---

## 9. Final Notes

- The **core** is the IR + `State` + `JuLSCore` + `FJCore`.
- The **algorithm layer** (ROAR style) is just plumbing and control logic; keep it dumb and generic.
- Start with **pure MIP** and a toy example; get the FJ+LS hybrid working end-to-end.
- Only then start adding:
  - Better neighborhoods.
  - LNS framework.
  - Flowty-specific graph/subproblem constructs.

You can feed each phase's "Tasks" and "Prompts" into a coding LLM and iterate with tests in between. That's how you keep the thing from turning into a mess.
