find_package(Python 3.9 REQUIRED COMPONENTS Interpreter Development.Module)

nanobind_add_module(_coso
    NB_STATIC
    bindings.cpp
)

target_link_libraries(_coso PRIVATE coso)

# Place the compiled extension module inside the coso/ package directory
# so that `from coso._coso import ...` works at build time.
set_target_properties(_coso PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/coso"
)

install(TARGETS _coso DESTINATION coso)
