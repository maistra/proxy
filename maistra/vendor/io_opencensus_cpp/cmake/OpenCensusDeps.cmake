# Copyright 2018, OpenCensus Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest
  GIT_TAG master)
FetchContent_Declare(
  abseil
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp
  GIT_TAG master)
FetchContent_Declare(
  prometheus
  GIT_REPOSITORY https://github.com/jupp0r/prometheus-cpp
  GIT_TAG master)
FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark
  GIT_TAG main)

FetchContent_GetProperties(googletest)
if(BUILD_TESTING)
  message(STATUS "Dependency: googletest (BUILD_TESTING=${BUILD_TESTING})")
  if(NOT googletest_POPULATED)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
      # All the libraries in the build must use either /MD or /MT (runtime
      # library to link)
      #
      # force this option to ON so that Google Test will use /MD instead of /MT
      # /MD is now the default for Visual Studio, so it should be our default,
      # too
      option(
        gtest_force_shared_crt
        "Use shared (DLL) run-time lib even when Google Test is built as static lib."
        ON)
    endif()

    FetchContent_Populate(googletest)
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR}
                     EXCLUDE_FROM_ALL)
  endif()
endif()

FetchContent_GetProperties(abseil)
if(NOT abseil_POPULATED)
  message(STATUS "Dependency: abseil")
  set(orig_BUILD_TESTING "${BUILD_TESTING}")
  set(BUILD_TESTING OFF) # Don't include abseil tests.
  FetchContent_Populate(abseil)
  add_subdirectory(${abseil_SOURCE_DIR} ${abseil_BINARY_DIR} EXCLUDE_FROM_ALL)
  set(BUILD_TESTING "${orig_BUILD_TESTING}") # Restore value.
endif()

FetchContent_GetProperties(prometheus)
if(NOT prometheus_POPULATED)
  message(STATUS "Dependency: prometheus")
  set(ENABLE_PUSH
      OFF
      CACHE BOOL "Build prometheus-cpp push library" FORCE)
  set(ENABLE_PULL
      OFF
      CACHE BOOL "Build prometheus-cpp pull library" FORCE)
  set(ENABLE_COMPRESSION
      OFF
      CACHE BOOL "Enable gzip compression for prometheus-cpp" FORCE)
  set(ENABLE_TESTING
      OFF
      CACHE BOOL "Build test for prometheus-cpp" FORCE)
  FetchContent_Populate(prometheus)
  add_subdirectory(${prometheus_SOURCE_DIR} ${prometheus_BINARY_DIR}
                   EXCLUDE_FROM_ALL)
endif()

FetchContent_GetProperties(benchmark)
if(BUILD_TESTING)
  message(STATUS "Dependency: benchmark (BUILD_TESTING=${BUILD_TESTING})")
  if(NOT benchmark_POPULATED)
    set(BENCHMARK_ENABLE_TESTING
        OFF
        CACHE BOOL "Enable testing of the benchmark library." FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS
        OFF
        CACHE BOOL "Enable building the unit tests which depend on gtest" FORCE)
    FetchContent_Populate(benchmark)
    add_subdirectory(${benchmark_SOURCE_DIR} ${benchmark_BINARY_DIR}
                     EXCLUDE_FROM_ALL)
  endif()
endif()
