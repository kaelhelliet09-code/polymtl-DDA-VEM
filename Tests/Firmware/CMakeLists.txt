add_executable(hardware_revision_tests
    hardware_revision_tests.cpp
)

target_include_directories(hardware_revision_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/Application
)

dda_enable_strict_warnings(hardware_revision_tests AS_ERRORS CONVERSIONS)

add_test(NAME hardware_revision_tests COMMAND hardware_revision_tests)
