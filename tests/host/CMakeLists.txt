#
# AiBo test host: fake engine executable + fake cs gamedll
#
# SPDX-License-Identifier: Unlicense
#

# --- bot sources as a static lib (same TU list as the production .so) ---
set(AIBO_STATIC_SOURCES "")
foreach(s IN LISTS SRC)
    list(APPEND AIBO_STATIC_SOURCES "${PROJECT_SOURCE_DIR}/${s}")
endforeach()

add_library(aibo_static STATIC ${AIBO_STATIC_SOURCES})

target_compile_definitions(aibo_static PRIVATE YSTL_TESTS)

set_target_properties(aibo_static PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# production compile flags come from the root interface libs, linked below

# no process-wide dlsym detour in the harness
target_compile_definitions(aibo_static PRIVATE LINKENT_STATIC)

# --- fake cs gamedll (dlopen'ed by aibo_static at runtime, never linked) ---
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|ARM64|aarch64)$")
    set(FAKECS_SUFFIX "_arm64")
else()
    set(FAKECS_SUFFIX "_amd64")
endif()

if(WIN32)
    set(FAKECS_EXT ".dll")
elseif(APPLE)
    set(FAKECS_EXT ".dylib")
else()
    set(FAKECS_EXT ".so")
endif()

function(add_fake_cs_variant target)
    add_library(${target} SHARED fake_cs/fake_cs.cpp)

    set_target_properties(${target} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${target}"
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        # keep them exported for dlsym
        CXX_VISIBILITY_PRESET default
        VISIBILITY_INLINES_HIDDEN OFF
    )

    if(WIN32)
        set_target_properties(${target} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()

    target_include_directories(${target} PRIVATE
        "${PROJECT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/ext"
        "${PROJECT_SOURCE_DIR}/ext/ystl"
        "${PROJECT_SOURCE_DIR}/ext/linkage"
    )

    if(CLANG_OR_GCC)
        target_compile_options(${target} PRIVATE -Wall -Wextra -fPIC)
    endif()

    if(WIN32)
        target_link_libraries(${target} PRIVATE ws2_32)
    endif()
endfunction()

add_fake_cs_variant(fake_cs)
add_fake_cs_variant(fake_cs_modern)
add_fake_cs_variant(fake_cs_regame)
target_compile_definitions(fake_cs PRIVATE YSTL_TESTS)
target_compile_definitions(fake_cs_modern PRIVATE YSTL_TESTS)
target_compile_definitions(fake_cs_regame PRIVATE YSTL_TESTS)
target_compile_definitions(fake_cs_modern PRIVATE FAKECS_MODERN=1)
target_compile_definitions(fake_cs_regame PRIVATE FAKECS_MODERN=1 FAKECS_REGAME=1)

# stage each variant where Game::loadCSBinary looks
set(TEST_GAMEDIR "${CMAKE_CURRENT_BINARY_DIR}/gamedir")

function(stage_fake_cs target subdir)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/${subdir}/cstrike/dlls"
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${target}>
            "${CMAKE_CURRENT_BINARY_DIR}/${subdir}/cstrike/dlls/cs${FAKECS_SUFFIX}${FAKECS_EXT}"
        COMMENT "Staging ${target} into ${subdir}"
        VERBATIM
    )
endfunction()

stage_fake_cs(fake_cs gamedir)
stage_fake_cs(fake_cs_modern gamedir-modern)
stage_fake_cs(fake_cs_regame gamedir-regame)

# --- test host executable (fake engine + scenarios) ---
file(GLOB AIBO_TEST_SCENARIOS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scenarios/*.cpp")

add_executable(aibo_testhost
    host_main.cpp
    fake_engine.cpp
    ${AIBO_TEST_SCENARIOS}
)

set_target_properties(aibo_testhost PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if(MSVC)
  # match the static CRT of aibo_static
  set_property(TARGET aibo_testhost PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_include_directories(aibo_testhost PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_BINARY_DIR}"
)

target_include_directories(aibo_testhost SYSTEM PRIVATE
    "${PROJECT_SOURCE_DIR}/inc"
    "${PROJECT_SOURCE_DIR}/ext"
    "${PROJECT_SOURCE_DIR}/ext/ystl"
    "${PROJECT_SOURCE_DIR}/ext/linkage"
)

if(CLANG_OR_GCC)
    target_compile_options(aibo_testhost PRIVATE -Wall -Wextra)
endif()
target_compile_definitions(aibo_testhost PRIVATE
  "AIBO_TEST_GAMEDIR=\"${TEST_GAMEDIR}\""
  "AIBO_TEST_CSDLL=\"${TEST_GAMEDIR}/cstrike/dlls/cs${FAKECS_SUFFIX}${FAKECS_EXT}\""
  "AIBO_TEST_CSSUFFIX=\"cs${FAKECS_SUFFIX}${FAKECS_EXT}\""
  YSTL_TESTS
)

# the final link happens here, so repeat it for the exe
if(AIBO_SANITIZE AND CMAKE_BUILD_TYPE MATCHES "Debug")
  target_compile_options(aibo_testhost PRIVATE -fsanitize=${AIBO_SANITIZE} -fno-omit-frame-pointer -O1)
  # The production bot intentionally uses -fno-rtti; the executable must not
  # request UBSan vptr metadata that the bot library therefore cannot emit.
  target_compile_options(aibo_testhost PRIVATE -fno-sanitize=vptr)
  target_link_options(aibo_testhost PRIVATE -fsanitize=${AIBO_SANITIZE} -fno-omit-frame-pointer -O1)
endif()

if(WIN32)
    target_link_libraries(aibo_testhost PRIVATE aibo_static user32 ws2_32)
else()
    target_link_libraries(aibo_testhost PRIVATE aibo_static dl m pthread)
endif()

add_dependencies(aibo_testhost fake_cs fake_cs_modern fake_cs_regame)

# one ctest entry per scenario: singletons have no reset,
# so scenarios must never share a process
set(DISCOVERED_TESTS "${CMAKE_CURRENT_BINARY_DIR}/discovered_tests.cmake")

# ctest errors on a missing include, so seed an empty one before the first build
file(WRITE "${DISCOVERED_TESTS}" "# pending aibo_testhost build\n")

# discovery reruns whenever the host or a staged game dll changes, so adding
# a scenario never requires touching CMake: drop the .cpp, rebuild, ctest sees it
add_custom_command(
    OUTPUT "${DISCOVERED_TESTS}"
    COMMAND ${CMAKE_COMMAND}
        "-DTEST_EXECUTABLE=$<TARGET_FILE:aibo_testhost>"
        "-DTEST_OUTPUT=${DISCOVERED_TESTS}"
        "-DTEST_GAMEDIR=${TEST_GAMEDIR}"
        "-DTEST_WINDOWS=${WIN32}"
        "-DTEST_CSSUFFIX=cs${FAKECS_SUFFIX}${FAKECS_EXT}"
        -P "${PROJECT_SOURCE_DIR}/tests/cmake/DiscoverTests.cmake"
    DEPENDS aibo_testhost fake_cs fake_cs_modern fake_cs_regame
    COMMENT "Discovering aibo test cases"
    VERBATIM
)

add_custom_target(discover_aibo_tests ALL DEPENDS "${DISCOVERED_TESTS}")

set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${DISCOVERED_TESTS}")

add_custom_target(run-tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS discover_aibo_tests fake_cs
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running aibo test host"
    USES_TERMINAL
    VERBATIM
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set_source_files_properties(
    "${PROJECT_SOURCE_DIR}/src/copycat/copycat_core.cpp"
    "${PROJECT_SOURCE_DIR}/src/copycat/copycat_lite.cpp"
    "${PROJECT_SOURCE_DIR}/src/copycat/copycat_routes.cpp"
    "${PROJECT_SOURCE_DIR}/src/copycat/copycat_learning.cpp"
    "${PROJECT_SOURCE_DIR}/src/copycat/copycat_explore.cpp"
    "${PROJECT_SOURCE_DIR}/src/copycat/copycat_dialogue.cpp"
    "${PROJECT_SOURCE_DIR}/src/copycat/copycat_final.cpp"
    PROPERTIES COMPILE_OPTIONS "-w")
  set_source_files_properties("${PROJECT_SOURCE_DIR}/src/human_copycat.cpp" "${PROJECT_SOURCE_DIR}/src/human_explore.cpp" "${PROJECT_SOURCE_DIR}/src/human_final.cpp" "${PROJECT_SOURCE_DIR}/src/human_chat.cpp"
    PROPERTIES COMPILE_OPTIONS "-Wno-effc++;-Wno-zero-as-null-pointer-constant")
endif()
