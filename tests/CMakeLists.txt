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
