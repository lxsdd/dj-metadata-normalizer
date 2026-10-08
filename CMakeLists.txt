cmake_minimum_required(VERSION 3.24)

project(dj_metadata_normalizer VERSION 0.1.0 LANGUAGES CXX)

add_library(djmeta_core STATIC
    src/core/normalizer.cpp
    src/core/batch_plan.cpp
    src/core/rule_loader.cpp
    src/core/analysis_json.cpp
)
target_include_directories(djmeta_core PUBLIC include)
target_compile_features(djmeta_core PUBLIC cxx_std_20)
set_target_properties(djmeta_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(djmeta_core PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(djmeta_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_library(djmeta_native SHARED
    src/interop/c_api.cpp
)
target_link_libraries(djmeta_native PRIVATE djmeta_core)
target_include_directories(djmeta_native PUBLIC include)
target_compile_features(djmeta_native PUBLIC cxx_std_20)
target_compile_definitions(djmeta_native PRIVATE DJMETA_BUILD_DLL=1)

if(MSVC)
    target_compile_options(djmeta_native PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(djmeta_native PRIVATE -Wall -Wextra -Wpedantic -Werror)
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

add_executable(djmeta_c_api_tests
    tests/c_api_tests.cpp
)
target_link_libraries(djmeta_c_api_tests PRIVATE djmeta_native)
target_include_directories(djmeta_c_api_tests PRIVATE include)
target_compile_features(djmeta_c_api_tests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(djmeta_c_api_tests PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(djmeta_c_api_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(djmeta_batch_plan_tests
    tests/batch_plan_tests.cpp
)
target_link_libraries(djmeta_batch_plan_tests PRIVATE djmeta_core)
target_compile_features(djmeta_batch_plan_tests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(djmeta_batch_plan_tests PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(djmeta_batch_plan_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

enable_testing()
add_test(NAME djmeta_batch_plan_tests COMMAND djmeta_batch_plan_tests)
add_test(NAME djmeta_core_tests COMMAND djmeta_core_tests)
set_tests_properties(djmeta_core_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME djmeta_c_api_tests COMMAND djmeta_c_api_tests)
