#
# AiBo test suite (fake engine harness)
#
# SPDX-License-Identifier: Unlicense
#
# Native gcc/clang/msvc, x64 only. Requires 64-bit (string_t layout) and
# skips TLS (vendored mbedtls is unnecessary weight for the harness).
#

if(NOT AIBO_64BIT)
  message(FATAL_ERROR "AIBO_BUILD_TESTS requires -DAIBO_64BIT=ON (32-bit string_t layout is unsupported by the harness)")
endif()

add_subdirectory(host)

add_executable(copycat_aim aim_test.cpp
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_aim.cpp"
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_core.cpp"
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_lite.cpp")
set_target_properties(copycat_aim PROPERTIES CXX_STANDARD 20)
add_test(NAME copycat_aim COMMAND copycat_aim)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_executable(aim_baseline_current aim_baseline_trace.cpp
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_core.cpp" "${PROJECT_SOURCE_DIR}/src/copycat/copycat_lite.cpp")
target_include_directories(aim_baseline_current PRIVATE "${PROJECT_SOURCE_DIR}/src/copycat")
set_target_properties(aim_baseline_current PROPERTIES CXX_STANDARD 20)
add_test(NAME aim_baseline_compatibility COMMAND ${Python3_EXECUTABLE}
  "${CMAKE_CURRENT_SOURCE_DIR}/compare_aim_baseline.py" "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/aim_baseline.csv" $<TARGET_FILE:aim_baseline_current>)

add_executable(copycat_final final_test.cpp
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_final.cpp"
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_explore.cpp"
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_dialogue.cpp"
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_core.cpp"
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_lite.cpp")
set_target_properties(copycat_final PROPERTIES CXX_STANDARD 20)
add_test(NAME copycat_final COMMAND copycat_final)

add_executable(explore_dialogue explore_dialogue_test.cpp
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_explore.cpp"
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_dialogue.cpp"
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_core.cpp"
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_lite.cpp")
set_target_properties(explore_dialogue PROPERTIES CXX_STANDARD 20)
add_test(NAME explore_dialogue COMMAND explore_dialogue)

add_executable(copycat_learning learning_test.cpp
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_learning.cpp"
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_core.cpp"
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_lite.cpp")
set_target_properties(copycat_learning PROPERTIES CXX_STANDARD 20)
add_test(NAME copycat_learning COMMAND copycat_learning)

add_executable(rbcc_portable rbcc_test.cpp
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_core.cpp"
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_lite.cpp")
add_executable(rbcc_lite rbcc_lite_test.cpp
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_core.cpp"
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_lite.cpp")
set_target_properties(rbcc_portable rbcc_lite PROPERTIES CXX_STANDARD 11)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/rbcc/build/tests")
add_test(NAME rbcc_portable COMMAND rbcc_portable)
add_test(NAME rbcc_lite COMMAND rbcc_lite)
set_tests_properties(rbcc_portable rbcc_lite PROPERTIES
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/rbcc")

add_executable(rbcc_routes rbcc_routes_test.cpp
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_routes.cpp"
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_core.cpp"
  "${PROJECT_SOURCE_DIR}/src/copycat/copycat_lite.cpp")
set_target_properties(rbcc_routes PROPERTIES CXX_STANDARD 11)
add_test(NAME rbcc_routes COMMAND rbcc_routes)
set_tests_properties(rbcc_routes PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/rbcc")
if(AIBO_SANITIZE)
  foreach(rbcc_target rbcc_portable rbcc_lite rbcc_routes copycat_learning explore_dialogue copycat_final copycat_aim)
    target_compile_options(${rbcc_target} PRIVATE -fsanitize=${AIBO_SANITIZE} -fno-omit-frame-pointer -O1)
    target_link_options(${rbcc_target} PRIVATE -fsanitize=${AIBO_SANITIZE})
  endforeach()
endif()
