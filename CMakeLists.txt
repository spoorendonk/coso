cmake_minimum_required(VERSION 3.25)
project(coso
    VERSION 0.1.0
    DESCRIPTION "Combinatorial Structure-aware Optimization"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --------------------------------------------------------------------------- #
#  Options                                                                     #
# --------------------------------------------------------------------------- #

option(COSO_BUILD_TESTS   "Build Catch2 unit tests"          ON)
option(COSO_BUILD_PYTHON  "Build nanobind Python bindings"   OFF)
option(COSO_USE_TBB       "Enable TBB for parallel solving"  OFF)

# --------------------------------------------------------------------------- #
#  Dependencies                                                                #
# --------------------------------------------------------------------------- #

include(FetchContent)

# Catch2 v3.7.1
if(COSO_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()

# nanobind (optional Python bindings)
if(COSO_BUILD_PYTHON)
    FetchContent_Declare(
        nanobind
        GIT_REPOSITORY https://github.com/wjakob/nanobind.git
        GIT_TAG        v2.4.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(nanobind)
endif()

# TBB (optional, system-installed)
if(COSO_USE_TBB)
    find_package(TBB REQUIRED)
endif()

# --------------------------------------------------------------------------- #
#  Source library                                                              #
# --------------------------------------------------------------------------- #

add_library(coso STATIC
    src/model/routing_model.cpp
    src/model/schedule_model.cpp
    src/model/assignment_model.cpp
    src/model/packing_model.cpp
    src/model/instance_reader.cpp
    src/routing/problem_data.cpp
    src/routing/route.cpp
    src/routing/solution.cpp
    src/routing/cost_evaluator.cpp
    src/routing/piecewise_cost.cpp
    src/routing/construction.cpp
    src/routing/operators/exchange.cpp
    src/routing/operators/swap_star.cpp
    src/routing/operators/route_split.cpp
    src/routing/operators/insert_optional.cpp
    src/routing/operators/pair_operators.cpp
    src/routing/operators/relocate_with_depot.cpp
    src/routing/local_search.cpp
    src/search/stop_criterion.cpp
    src/search/acceptance.cpp
    src/search/iterated_local_search.cpp
    src/search/operator_selector.cpp
    src/search/penalty_manager.cpp
    src/search/guided_local_search.cpp
    src/search/population.cpp
    src/search/score_assert.cpp
    src/search/crossover.cpp
    src/search/genetic_algorithm.cpp
    src/search/solution_finalizer.cpp
    src/search/portfolio.cpp
    src/packing/packing_solution.cpp
    src/packing/packing_operators.cpp
    src/packing/bin_capacity.cpp
    src/routing/overconstrained.cpp
    src/assignment/overconstrained.cpp
    src/assignment/construction.cpp
    src/assignment/replanning.cpp
    src/assignment/parsers.cpp
    src/assignment/constraints/constraint.cpp
    src/assignment/constraints/automaton.cpp
    src/scheduling/parsers.cpp
    src/scheduling/schedule_data.cpp
    src/scheduling/construction.cpp
    src/scheduling/disjunctive_graph.cpp
    src/scheduling/schedule_solution.cpp
    src/scheduling/mode_selection.cpp
    src/scheduling/schedule_operators.cpp
    src/scheduling/perturbation.cpp
    src/scheduling/setup_times.cpp
    src/scheduling/calendar.cpp
    src/lotsizing/lotsizing_data.cpp
    src/lotsizing/lotsizing_solution.cpp
    src/lotsizing/lotsizing_operators.cpp
    src/lotsizing/construction.cpp
    src/search/benchmarker.cpp
    src/search/warm_start.cpp
    src/search/daemon.cpp
    src/search/partitioned_search.cpp
    src/network/network_data.cpp
    src/network/network_solution.cpp
    src/network/mcf_solver.cpp
    src/network/construction.cpp
    src/network/network_operators.cpp
)
target_include_directories(coso PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(coso PUBLIC cxx_std_23)

# When building Python bindings, the static lib must be PIC-compatible.
if(COSO_BUILD_PYTHON)
    set_target_properties(coso PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

if(COSO_USE_TBB)
    target_link_libraries(coso PUBLIC TBB::tbb)
    target_compile_definitions(coso PUBLIC COSO_USE_TBB)
endif()

# --------------------------------------------------------------------------- #
#  CLI tool                                                                    #
# --------------------------------------------------------------------------- #

add_executable(coso-solve src/cli/main.cpp)
target_link_libraries(coso-solve PRIVATE coso)

# --------------------------------------------------------------------------- #
#  Tests                                                                       #
# --------------------------------------------------------------------------- #

if(COSO_BUILD_TESTS)
    enable_testing()
    include(CTest)
    include(Catch2WithMain OPTIONAL)
    add_subdirectory(tests)
endif()

# --------------------------------------------------------------------------- #
#  Python bindings                                                             #
# --------------------------------------------------------------------------- #

if(COSO_BUILD_PYTHON)
    add_subdirectory(python)
endif()
