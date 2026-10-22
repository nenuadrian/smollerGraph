cmake_minimum_required(VERSION 3.28)
project(smollergraph VERSION 0.1.0 LANGUAGES CXX)

# Without a build type, CMake builds without optimizations, which is no way to
# measure a database, so an optimized build is the default.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# With Clang, CMake looks for C++20 modules, which we don't use.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
# Abseil, Google's C++ library, for absl::Status and its string functions.
set(ABSL_PROPAGATE_CXX_STD ON)
FetchContent_Declare(
  abseil
  URL https://github.com/abseil/abseil-cpp/archive/refs/tags/20250512.1.tar.gz
  SYSTEM
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(abseil)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
)
FetchContent_MakeAvailable(googletest)
# sg serve answers HTTP with cpp-httplib, a library in one header, without
# HTTPS or compression.
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF)
set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF)
FetchContent_Declare(
  httplib
  URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.60.1.tar.gz
)
FetchContent_MakeAvailable(httplib)
enable_testing()

add_compile_options(-Wall -Wextra -Wpedantic)

add_library(smollergraph src/graph.cc src/facts.cc src/query.cc src/pager.cc
                         src/api.cc)
target_include_directories(smollergraph PUBLIC src)
target_link_libraries(smollergraph PUBLIC absl::status absl::statusor
                      absl::strings)

add_executable(sg src/main.cc src/server.cc)
target_link_libraries(sg PRIVATE smollergraph httplib::httplib)
target_compile_definitions(sg PRIVATE SMOLLERGRAPH_UI="${CMAKE_SOURCE_DIR}/ui")

add_executable(smollergraph_tests tests/graph_test.cc tests/pager_test.cc
                                  tests/record_file_test.cc tests/api_test.cc)
target_link_libraries(smollergraph_tests PRIVATE smollergraph GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(smollergraph_tests)

add_executable(make_friends tools/make_friends.cc)
