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
