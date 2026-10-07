cmake_minimum_required(VERSION 3.24)

project(dj_metadata_normalizer VERSION 0.1.0 LANGUAGES CXX)

add_library(djmeta_core STATIC
    src/core/normalizer.cpp
    src/core/rule_loader.cpp
)
target_include_directories(djmeta_core PUBLIC include)
target_compile_features(djmeta_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(djmeta_core PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(djmeta_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(djmeta_core_tests
    tests/core_tests.cpp
)
target_link_libraries(djmeta_core_tests PRIVATE djmeta_core)
target_compile_features(djmeta_core_tests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(djmeta_core_tests PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(djmeta_core_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

enable_testing()
add_test(NAME djmeta_core_tests COMMAND djmeta_core_tests)
set_tests_properties(djmeta_core_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
