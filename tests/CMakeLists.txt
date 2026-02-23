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
