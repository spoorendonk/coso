# --------------------------------------------------------------------------- #
#  Model API contract tests                                                    #
# --------------------------------------------------------------------------- #

add_executable(model_test model/model_test.cpp)
target_link_libraries(model_test PRIVATE coso Catch2::Catch2WithMain)

include(Catch2WithMain OPTIONAL)
include(CatchSharedTests OPTIONAL)

# Register Catch2 tests with CTest.
if(COMMAND catch_discover_tests)
    catch_discover_tests(model_test)
else()
    add_test(NAME model_test COMMAND model_test)
endif()

# --------------------------------------------------------------------------- #
#  CVRPLIB instance reader tests                                               #
# --------------------------------------------------------------------------- #

add_executable(instance_reader_test model/instance_reader_test.cpp)
target_link_libraries(instance_reader_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(instance_reader_test)
else()
    add_test(NAME instance_reader_test COMMAND instance_reader_test)
endif()

# --------------------------------------------------------------------------- #
#  Routing engine tests                                                        #
# --------------------------------------------------------------------------- #

add_executable(problem_data_test routing/problem_data_test.cpp)
target_link_libraries(problem_data_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(problem_data_test)
else()
    add_test(NAME problem_data_test COMMAND problem_data_test)
endif()

# --------------------------------------------------------------------------- #
#  Route + LoadResource tests                                                  #
# --------------------------------------------------------------------------- #

add_executable(route_test routing/route_test.cpp)
target_link_libraries(route_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(route_test)
else()
    add_test(NAME route_test COMMAND route_test)
endif()

# --------------------------------------------------------------------------- #
#  DurationResource tests                                                      #
# --------------------------------------------------------------------------- #

add_executable(duration_resource_test routing/duration_resource_test.cpp)
target_link_libraries(duration_resource_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(duration_resource_test)
else()
    add_test(NAME duration_resource_test COMMAND duration_resource_test)
endif()

# --------------------------------------------------------------------------- #
#  PrecedenceResource tests                                                    #
# --------------------------------------------------------------------------- #

add_executable(precedence_resource_test routing/precedence_resource_test.cpp)
target_link_libraries(precedence_resource_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(precedence_resource_test)
else()
    add_test(NAME precedence_resource_test COMMAND precedence_resource_test)
endif()

# --------------------------------------------------------------------------- #
#  SkillFilter tests                                                           #
# --------------------------------------------------------------------------- #

add_executable(skill_filter_test routing/skill_filter_test.cpp)
target_link_libraries(skill_filter_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(skill_filter_test)
else()
    add_test(NAME skill_filter_test COMMAND skill_filter_test)
endif()

# --------------------------------------------------------------------------- #
#  Type incompatibility resource tests                                         #
# --------------------------------------------------------------------------- #

add_executable(type_incompatibility_test routing/type_incompatibility_test.cpp)
target_link_libraries(type_incompatibility_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(type_incompatibility_test)
else()
    add_test(NAME type_incompatibility_test COMMAND type_incompatibility_test)
endif()

# --------------------------------------------------------------------------- #
#  BreakResource tests                                                         #
# --------------------------------------------------------------------------- #

add_executable(break_resource_test routing/break_resource_test.cpp)
target_link_libraries(break_resource_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(break_resource_test)
else()
    add_test(NAME break_resource_test COMMAND break_resource_test)
endif()

# --------------------------------------------------------------------------- #
#  DepotResource tests                                                         #
# --------------------------------------------------------------------------- #

add_executable(depot_resource_test routing/depot_resource_test.cpp)
target_link_libraries(depot_resource_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(depot_resource_test)
else()
    add_test(NAME depot_resource_test COMMAND depot_resource_test)
endif()

# --------------------------------------------------------------------------- #
#  Solution tests                                                              #
# --------------------------------------------------------------------------- #

add_executable(solution_test routing/solution_test.cpp)
target_link_libraries(solution_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(solution_test)
else()
    add_test(NAME solution_test COMMAND solution_test)
endif()

# --------------------------------------------------------------------------- #
#  CostEvaluator tests                                                         #
# --------------------------------------------------------------------------- #

add_executable(cost_evaluator_test routing/cost_evaluator_test.cpp)
target_link_libraries(cost_evaluator_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(cost_evaluator_test)
else()
    add_test(NAME cost_evaluator_test COMMAND cost_evaluator_test)
endif()

# --------------------------------------------------------------------------- #
#  Construction heuristic tests                                                #
# --------------------------------------------------------------------------- #

add_executable(construction_test routing/construction_test.cpp)
target_link_libraries(construction_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(construction_test)
else()
    add_test(NAME construction_test COMMAND construction_test)
endif()

# --------------------------------------------------------------------------- #
#  Exchange operator tests                                                     #
# --------------------------------------------------------------------------- #

add_executable(exchange_test routing/exchange_test.cpp)
target_link_libraries(exchange_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(exchange_test)
else()
    add_test(NAME exchange_test COMMAND exchange_test)
endif()

# --------------------------------------------------------------------------- #
#  SWAP* operator tests                                                        #
# --------------------------------------------------------------------------- #

add_executable(swap_star_test routing/swap_star_test.cpp)
target_link_libraries(swap_star_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(swap_star_test)
else()
    add_test(NAME swap_star_test COMMAND swap_star_test)
endif()

# --------------------------------------------------------------------------- #
#  Route split operator tests                                                  #
# --------------------------------------------------------------------------- #

add_executable(route_split_test routing/route_split_test.cpp)
target_link_libraries(route_split_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(route_split_test)
else()
    add_test(NAME route_split_test COMMAND route_split_test)
endif()

# --------------------------------------------------------------------------- #
#  Insert/remove optional client operator tests                                #
# --------------------------------------------------------------------------- #

add_executable(insert_optional_test routing/insert_optional_test.cpp)
target_link_libraries(insert_optional_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(insert_optional_test)
else()
    add_test(NAME insert_optional_test COMMAND insert_optional_test)
endif()

# --------------------------------------------------------------------------- #
#  Pair operators (pickup-delivery) tests                                      #
# --------------------------------------------------------------------------- #

add_executable(pair_operators_test routing/pair_operators_test.cpp)
target_link_libraries(pair_operators_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(pair_operators_test)
else()
    add_test(NAME pair_operators_test COMMAND pair_operators_test)
endif()

# --------------------------------------------------------------------------- #
#  RelocateWithDepot operator tests                                            #
# --------------------------------------------------------------------------- #

add_executable(relocate_with_depot_test routing/relocate_with_depot_test.cpp)
target_link_libraries(relocate_with_depot_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(relocate_with_depot_test)
else()
    add_test(NAME relocate_with_depot_test COMMAND relocate_with_depot_test)
endif()

# --------------------------------------------------------------------------- #
#  Distance resource tests                                                     #
# --------------------------------------------------------------------------- #

add_executable(distance_resource_test routing/distance_resource_test.cpp)
target_link_libraries(distance_resource_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(distance_resource_test)
else()
    add_test(NAME distance_resource_test COMMAND distance_resource_test)
endif()

# --------------------------------------------------------------------------- #
#  Task count resource tests                                                   #
# --------------------------------------------------------------------------- #

add_executable(task_count_resource_test routing/task_count_resource_test.cpp)
target_link_libraries(task_count_resource_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(task_count_resource_test)
else()
    add_test(NAME task_count_resource_test COMMAND task_count_resource_test)
endif()

# --------------------------------------------------------------------------- #
#  Local search engine tests                                                   #
# --------------------------------------------------------------------------- #

add_executable(local_search_test routing/local_search_test.cpp)
target_link_libraries(local_search_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(local_search_test)
else()
    add_test(NAME local_search_test COMMAND local_search_test)
endif()

# --------------------------------------------------------------------------- #
#  Stop criterion tests                                                        #
# --------------------------------------------------------------------------- #

add_executable(stop_criterion_test search/stop_criterion_test.cpp)
target_link_libraries(stop_criterion_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(stop_criterion_test)
else()
    add_test(NAME stop_criterion_test COMMAND stop_criterion_test)
endif()

# --------------------------------------------------------------------------- #
#  Operator selector tests                                                     #
# --------------------------------------------------------------------------- #

add_executable(operator_selector_test search/operator_selector_test.cpp)
target_link_libraries(operator_selector_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(operator_selector_test)
else()
    add_test(NAME operator_selector_test COMMAND operator_selector_test)
endif()

# --------------------------------------------------------------------------- #
#  Score corruption detection tests                                            #
# --------------------------------------------------------------------------- #

add_executable(score_assert_test search/score_assert_test.cpp)
target_link_libraries(score_assert_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(score_assert_test)
else()
    add_test(NAME score_assert_test COMMAND score_assert_test)
endif()

# --------------------------------------------------------------------------- #
#  Guided local search tests                                                   #
# --------------------------------------------------------------------------- #

add_executable(gls_test search/gls_test.cpp)
target_link_libraries(gls_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(gls_test)
else()
    add_test(NAME gls_test COMMAND gls_test)
endif()

# --------------------------------------------------------------------------- #
#  Acceptance criteria tests                                                   #
# --------------------------------------------------------------------------- #

add_executable(acceptance_test search/acceptance_test.cpp)
target_link_libraries(acceptance_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(acceptance_test)
else()
    add_test(NAME acceptance_test COMMAND acceptance_test)
endif()

# --------------------------------------------------------------------------- #
#  Penalty manager tests                                                       #
# --------------------------------------------------------------------------- #

add_executable(penalty_manager_test search/penalty_manager_test.cpp)
target_link_libraries(penalty_manager_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(penalty_manager_test)
else()
    add_test(NAME penalty_manager_test COMMAND penalty_manager_test)
endif()

# --------------------------------------------------------------------------- #
#  Iterated Local Search tests                                                 #
# --------------------------------------------------------------------------- #

add_executable(ils_test search/ils_test.cpp)
target_link_libraries(ils_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(ils_test)
else()
    add_test(NAME ils_test COMMAND ils_test)
endif()

# --------------------------------------------------------------------------- #
#  CLI tool tests                                                              #
# --------------------------------------------------------------------------- #

add_executable(cli_test cli/cli_test.cpp)
target_link_libraries(cli_test PRIVATE Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(cli_test
        PROPERTIES ENVIRONMENT "COSO_SOLVE_PATH=$<TARGET_FILE:coso-solve>"
    )
else()
    add_test(NAME cli_test COMMAND cli_test)
    set_property(TEST cli_test PROPERTY
        ENVIRONMENT "COSO_SOLVE_PATH=$<TARGET_FILE:coso-solve>"
    )
endif()

# --------------------------------------------------------------------------- #
#  Population tests                                                             #
# --------------------------------------------------------------------------- #

add_executable(population_test search/population_test.cpp)
target_link_libraries(population_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(population_test)
else()
    add_test(NAME population_test COMMAND population_test)
endif()

# --------------------------------------------------------------------------- #
#  Benchmarker harness tests                                                   #
# --------------------------------------------------------------------------- #

add_executable(benchmarker_test search/benchmarker_test.cpp)
target_link_libraries(benchmarker_test PRIVATE coso Catch2::Catch2WithMain)

# Unit tests (BKS lookup, CSV formatting) always run.
add_test(NAME benchmarker_test COMMAND benchmarker_test "~[benchmark]")

# Integration tests require downloaded instances; skip gracefully.
add_test(NAME benchmarker_integration_test
    COMMAND benchmarker_test "[benchmark]")
set_tests_properties(benchmarker_integration_test PROPERTIES
    ENVIRONMENT "COSO_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/data"
    SKIP_RETURN_CODE 4
    LABELS "benchmark"
)

# --------------------------------------------------------------------------- #
#  SREX crossover tests                                                        #
# --------------------------------------------------------------------------- #

add_executable(crossover_test search/crossover_test.cpp)
target_link_libraries(crossover_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(crossover_test)
else()
    add_test(NAME crossover_test COMMAND crossover_test)
endif()

# --------------------------------------------------------------------------- #
#  Solution finalizer tests                                                    #
# --------------------------------------------------------------------------- #

# --------------------------------------------------------------------------- #
#  Genetic algorithm tests                                                     #
# --------------------------------------------------------------------------- #

add_executable(genetic_algorithm_test search/genetic_algorithm_test.cpp)
target_link_libraries(genetic_algorithm_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(genetic_algorithm_test)
else()
    add_test(NAME genetic_algorithm_test COMMAND genetic_algorithm_test)
endif()

# --------------------------------------------------------------------------- #
#  Solution finalizer tests                                                    #
# --------------------------------------------------------------------------- #

add_executable(solution_finalizer_test search/solution_finalizer_test.cpp)
target_link_libraries(solution_finalizer_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(solution_finalizer_test)
else()
    add_test(NAME solution_finalizer_test COMMAND solution_finalizer_test)
endif()

# --------------------------------------------------------------------------- #
#  Portfolio solver tests                                                       #
# --------------------------------------------------------------------------- #

add_executable(portfolio_test search/portfolio_test.cpp)
target_link_libraries(portfolio_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(portfolio_test)
else()
    add_test(NAME portfolio_test COMMAND portfolio_test)
endif()

# --------------------------------------------------------------------------- #
#  Routing benchmark tests (require downloaded instances)                       #
# --------------------------------------------------------------------------- #

add_executable(benchmark_test routing/benchmark_test.cpp)
target_link_libraries(benchmark_test PRIVATE coso Catch2::Catch2WithMain)

# Use add_test (not catch_discover_tests) so we can set SKIP_RETURN_CODE.
# Catch2 returns exit code 4 when all tests are skipped; CTest must treat
# that as "skipped" rather than "failed".
add_test(NAME benchmark_test COMMAND benchmark_test)
set_tests_properties(benchmark_test PROPERTIES
    ENVIRONMENT "COSO_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/data"
    SKIP_RETURN_CODE 4
    LABELS "benchmark"
)

# --------------------------------------------------------------------------- #
#  VRPTW benchmark tests (require downloaded Solomon instances)                #
# --------------------------------------------------------------------------- #

add_executable(vrptw_benchmark_test routing/vrptw_benchmark_test.cpp)
target_link_libraries(vrptw_benchmark_test PRIVATE coso Catch2::Catch2WithMain)

add_test(NAME vrptw_benchmark_test COMMAND vrptw_benchmark_test)
set_tests_properties(vrptw_benchmark_test PROPERTIES
    ENVIRONMENT "COSO_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/data"
    SKIP_RETURN_CODE 4
    LABELS "benchmark"
)

# --------------------------------------------------------------------------- #
#  Warm start + pinning tests                                                  #
# --------------------------------------------------------------------------- #

add_executable(warm_start_test search/warm_start_test.cpp)
target_link_libraries(warm_start_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(warm_start_test)
else()
    add_test(NAME warm_start_test COMMAND warm_start_test)
endif()

# --------------------------------------------------------------------------- #
#  Schedule model tests                                                        #
# --------------------------------------------------------------------------- #

add_executable(schedule_model_test scheduling/schedule_model_test.cpp)
target_link_libraries(schedule_model_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(schedule_model_test)
else()
    add_test(NAME schedule_model_test COMMAND schedule_model_test)
endif()

# --------------------------------------------------------------------------- #
#  Disjunctive graph tests                                                     #
# --------------------------------------------------------------------------- #

add_executable(disjunctive_graph_test scheduling/disjunctive_graph_test.cpp)
target_link_libraries(disjunctive_graph_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(disjunctive_graph_test)
else()
    add_test(NAME disjunctive_graph_test COMMAND disjunctive_graph_test)
endif()

# --------------------------------------------------------------------------- #
#  Schedule solution tests                                                     #
# --------------------------------------------------------------------------- #

add_executable(schedule_solution_test scheduling/schedule_solution_test.cpp)
target_link_libraries(schedule_solution_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(schedule_solution_test)
else()
    add_test(NAME schedule_solution_test COMMAND schedule_solution_test)
endif()

# --------------------------------------------------------------------------- #
#  Scheduling construction heuristic tests                                     #
# --------------------------------------------------------------------------- #

add_executable(scheduling_construction_test scheduling/construction_test.cpp)
target_link_libraries(scheduling_construction_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(scheduling_construction_test)
else()
    add_test(NAME scheduling_construction_test COMMAND scheduling_construction_test)
endif()

# --------------------------------------------------------------------------- #
#  Mode selection tests                                                        #
# --------------------------------------------------------------------------- #

add_executable(mode_selection_test scheduling/mode_selection_test.cpp)
target_link_libraries(mode_selection_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(mode_selection_test)
else()
    add_test(NAME mode_selection_test COMMAND mode_selection_test)
endif()

# --------------------------------------------------------------------------- #
#  Scheduling parser tests                                                     #
# --------------------------------------------------------------------------- #

add_executable(scheduling_parser_test scheduling/parser_test.cpp)
target_link_libraries(scheduling_parser_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(scheduling_parser_test)
else()
    add_test(NAME scheduling_parser_test COMMAND scheduling_parser_test)
endif()

# --------------------------------------------------------------------------- #
#  Assignment model tests                                                      #
# --------------------------------------------------------------------------- #

add_executable(assignment_model_test assignment/assignment_model_test.cpp)
target_link_libraries(assignment_model_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(assignment_model_test)
else()
    add_test(NAME assignment_model_test COMMAND assignment_model_test)
endif()

# --------------------------------------------------------------------------- #
#  Assignment solution + cost evaluator tests                                  #
# --------------------------------------------------------------------------- #

add_executable(assignment_solution_test assignment/assignment_solution_test.cpp)
target_link_libraries(assignment_solution_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(assignment_solution_test)
else()
    add_test(NAME assignment_solution_test COMMAND assignment_solution_test)
endif()

# --------------------------------------------------------------------------- #
#  Assignment operator tests                                                   #
# --------------------------------------------------------------------------- #

add_executable(assignment_operators_test assignment/assignment_operators_test.cpp)
target_link_libraries(assignment_operators_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(assignment_operators_test)
else()
    add_test(NAME assignment_operators_test COMMAND assignment_operators_test)
endif()

# --------------------------------------------------------------------------- #
#  Assignment construction heuristic tests                                     #
# --------------------------------------------------------------------------- #

add_executable(assignment_construction_test assignment/construction_test.cpp)
target_link_libraries(assignment_construction_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(assignment_construction_test)
else()
    add_test(NAME assignment_construction_test COMMAND assignment_construction_test)
endif()

# --------------------------------------------------------------------------- #
#  Assignment parser tests                                                     #
# --------------------------------------------------------------------------- #

add_executable(assignment_parser_test assignment/parser_test.cpp)
target_link_libraries(assignment_parser_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(assignment_parser_test)
else()
    add_test(NAME assignment_parser_test COMMAND assignment_parser_test)
endif()

# --------------------------------------------------------------------------- #
#  Packing model tests                                                         #
# --------------------------------------------------------------------------- #

add_executable(packing_model_test packing/packing_model_test.cpp)
target_link_libraries(packing_model_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(packing_model_test)
else()
    add_test(NAME packing_model_test COMMAND packing_model_test)
endif()

# --------------------------------------------------------------------------- #
#  Packing solution tests                                                      #
# --------------------------------------------------------------------------- #

add_executable(packing_solution_test packing/packing_solution_test.cpp)
target_link_libraries(packing_solution_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(packing_solution_test)
else()
    add_test(NAME packing_solution_test COMMAND packing_solution_test)
endif()

# --------------------------------------------------------------------------- #
#  Packing operators tests                                                     #
# --------------------------------------------------------------------------- #

add_executable(packing_operators_test packing/packing_operators_test.cpp)
target_link_libraries(packing_operators_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(packing_operators_test)
else()
    add_test(NAME packing_operators_test COMMAND packing_operators_test)
endif()

# --------------------------------------------------------------------------- #
#  Schedule operators tests                                                    #
# --------------------------------------------------------------------------- #

add_executable(schedule_operators_test scheduling/schedule_operators_test.cpp)
target_link_libraries(schedule_operators_test PRIVATE coso Catch2::Catch2WithMain)

if(COMMAND catch_discover_tests)
    catch_discover_tests(schedule_operators_test)
else()
    add_test(NAME schedule_operators_test COMMAND schedule_operators_test)
endif()
