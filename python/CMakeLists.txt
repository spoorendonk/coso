find_package(Python 3.9 REQUIRED COMPONENTS Interpreter Development.Module)

nanobind_add_module(_coso
    NB_STATIC
    bindings.cpp
)

target_link_libraries(_coso PRIVATE coso)

# SYSTEM on FetchContent_Declare is not enough for nanobind: nanobind_add_module
# attaches nanobind's headers through its own targets with -I, so clang-tidy
# analyses them and bindings.cpp drowns in ~1500 findings from third-party code.
# Re-declare those interface includes as system headers. Python's headers arrive
# the same way and get the same treatment.
foreach(_nb_target nanobind-static nanobind-static-abi3 nanobind)
    if(TARGET ${_nb_target})
        get_target_property(_nb_inc ${_nb_target} INTERFACE_INCLUDE_DIRECTORIES)
        if(_nb_inc)
            set_target_properties(${_nb_target} PROPERTIES
                INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_nb_inc}")
        endif()
    endif()
endforeach()

if(TARGET Python::Module)
    get_target_property(_py_inc Python::Module INTERFACE_INCLUDE_DIRECTORIES)
    if(_py_inc)
        set_target_properties(Python::Module PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_py_inc}")
    endif()
endif()

# Place the compiled extension module inside the coso/ package directory
# so that `from coso._coso import ...` works at build time.
set_target_properties(_coso PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/coso"
)

install(TARGETS _coso DESTINATION coso)
