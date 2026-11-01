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
                         src/api.cc src/buffer_pool.cc src/name_index.cc
                         src/lexer.cc src/parser.cc src/plan.cc
                         src/token_store.cc src/embed.cc
                         src/export.cc)
target_include_directories(smollergraph PUBLIC src)
target_link_libraries(smollergraph PUBLIC absl::status absl::statusor
                      absl::strings)

add_executable(sg src/main.cc src/server.cc)
target_link_libraries(sg PRIVATE smollergraph httplib::httplib)
target_compile_definitions(sg PRIVATE SMOLLERGRAPH_UI="${CMAKE_SOURCE_DIR}/ui")

add_executable(smollergraph_tests tests/graph_test.cc tests/pager_test.cc
                                  tests/record_file_test.cc tests/api_test.cc
                                  tests/buffer_pool_test.cc
                                  tests/name_index_test.cc tests/lexer_test.cc
                                  tests/parser_test.cc tests/plan_test.cc
                                  tests/token_store_test.cc
                                  tests/path_test.cc tests/embed_test.cc
                                  tests/export_test.cc)
target_link_libraries(smollergraph_tests PRIVATE smollergraph GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(smollergraph_tests)

add_executable(make_friends tools/make_friends.cc)

# The API docs, from the comments in the headers, when Doxygen is installed:
# cmake --build build --target docs writes them to build/docs/html.
find_package(Doxygen)
if(DOXYGEN_FOUND)
  # doxygen-awesome, a theme that restyles Doxygen's pages, sidebar and all.
  FetchContent_Declare(
    doxygen_awesome
    URL https://github.com/jothepro/doxygen-awesome-css/archive/refs/tags/v2.5.0.tar.gz
  )
  FetchContent_MakeAvailable(doxygen_awesome)

  set(DOXYGEN_PROJECT_NAME smollerGraph)
  set(DOXYGEN_PROJECT_BRIEF "A small graph database, from scratch")
  set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/docs)
  set(DOXYGEN_USE_MDFILE_AS_MAINPAGE README.md)
  set(DOXYGEN_EXTRACT_ALL YES)
  set(DOXYGEN_JAVADOC_AUTOBRIEF YES)
  # A file's page draws the files that include it, but not all it includes,
  # which is most of the standard library.
  set(DOXYGEN_INCLUDE_GRAPH NO)
  set(DOXYGEN_GENERATE_TREEVIEW YES)
  set(DOXYGEN_DISABLE_INDEX NO)
  set(DOXYGEN_FULL_SIDEBAR NO)
  set(DOXYGEN_HTML_COLORSTYLE LIGHT)
  set(DOXYGEN_HTML_EXTRA_STYLESHEET
    ${doxygen_awesome_SOURCE_DIR}/doxygen-awesome.css
    ${doxygen_awesome_SOURCE_DIR}/doxygen-awesome-sidebar-only.css
  )
  # Our comments start with two slashes, as Google's style guide writes them,
  # and Doxygen reads comments written its own ways, such as with three, so a
  # filter gives each comment that starts a line in a header its third.
  set(DOXYGEN_FILTER_PATTERNS "*.h=sed -E 's#^ *//#&/#'")
  doxygen_add_docs(docs README.md src)
endif()

