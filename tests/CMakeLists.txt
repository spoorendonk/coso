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
