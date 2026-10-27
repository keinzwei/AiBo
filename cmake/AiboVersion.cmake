#
# AiBo version info from git and the host.
#
# SPDX-License-Identifier: Unlicense
#
# Provides in caller scope: GIT_FOUND, BUILD_* vars, VERSION_H (when git found).
#

find_package(Git QUIET)

if(NOT GIT_FOUND)
  return()
endif()

cmake_host_system_information(RESULT BUILD_MACHINE QUERY HOSTNAME)
execute_process(COMMAND git -C ${PROJECT_SOURCE_DIR} rev-parse --short HEAD OUTPUT_VARIABLE BUILD_HASH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git -C ${PROJECT_SOURCE_DIR} rev-list --count HEAD OUTPUT_VARIABLE BUILD_COUNT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git -C ${PROJECT_SOURCE_DIR} log --pretty="%ae" -1 OUTPUT_VARIABLE BUILD_AUTHOR OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)

# Source archives have no .git directory. Keep generated headers and the
# Windows numeric resource valid when Git itself is installed but rev-parse fails.
if(NOT BUILD_COUNT MATCHES "^[0-9]+$")
  set(BUILD_COUNT "1")
  set(BUILD_HASH "7985904")
  set(BUILD_AUTHOR "\"AiBo Team / RealBot contributors\"")
endif()

set(BUILD_COMPILER ${CMAKE_CXX_COMPILER_ID}\ ${CMAKE_CXX_COMPILER_VERSION})
set(BUILD_VERSION ${PROJECT_VERSION})

# configure-time stamp (keeps __DATE__ out of headers, clang pch-safe)
string(TIMESTAMP BUILD_DATE "%b %d %Y")
string(TIMESTAMP BUILD_TIME "%H:%M:%S")
string(TIMESTAMP BUILD_YEAR "%Y")

if(WIN32)
  string(REPLACE . , BUILD_WINVER ${PROJECT_VERSION})
else()
  set(BUILD_WINVER ${BUILD_VERSION})
endif()

configure_file(${PROJECT_SOURCE_DIR}/inc/version.h.in version.build.h @ONLY)
set(VERSION_H "${PROJECT_BINARY_DIR}/version.build.h")
