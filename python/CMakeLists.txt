find_package(Python 3.9 REQUIRED COMPONENTS Interpreter Development.Module)

nanobind_add_module(_coso
    NB_STATIC
    bindings.cpp
)

target_link_libraries(_coso PRIVATE coso)

install(TARGETS _coso DESTINATION coso)
