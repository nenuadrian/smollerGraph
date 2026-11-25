cmake_minimum_required(VERSION 3.28)
project(smollergraph VERSION 0.20.2 LANGUAGES CXX)

# Built on its own, smollerGraph is the whole project, and it picks the build
# type and the standard. Fetched by another project, those are that project's
# to pick.
if(PROJECT_IS_TOP_LEVEL)
  # Without a build type, CMake builds without optimizations, which is no way
  # to measure a database, so an optimized build is the default.
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
  endif()
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  # MSVC's programs need Microsoft's C++ runtime installed, unless they carry
  # their own copy of it, as sg does. Abseil picks the installed one unless
  # it's told otherwise, and the two can't be linked together.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  set(ABSL_MSVC_STATIC_RUNTIME ON)
endif()
# With Clang, CMake looks for C++20 modules, which we don't use.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

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

# GCC and Clang take the same warning flags, and MSVC has its own. /utf-8
# reads our sources as UTF-8, as they're written, where MSVC would read them in
# the system's code page.
if(MSVC)
  add_compile_options(/W4 /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(smollergraph src/graph.cc src/facts.cc src/query.cc src/pager.cc
                         src/api.cc src/buffer_pool.cc src/name_index.cc
                         src/lexer.cc src/parser.cc src/plan.cc
                         src/token_store.cc src/embed.cc
                         src/export.cc)
# The name another project links: CMake knows a name with :: in it is a
# target, and says so when it's missing, where it would take a plain name it
# can't find for a library file to look for.
add_library(smollergraph::smollergraph ALIAS smollergraph)
# The headers are in include/smollergraph, so another project includes them as
# "smollergraph/graph.h", and our names, such as graph.h, can't clash with its
# own.
target_include_directories(smollergraph PUBLIC include)
target_link_libraries(smollergraph PUBLIC absl::status absl::statusor
                      absl::strings)
# C++20 at least, for the library and for whatever links it, which can ask for
# a newer standard.
target_compile_features(smollergraph PUBLIC cxx_std_20)

# sg, which another project builds when it names it, as in cmake --build build
# --target sg, and not otherwise. A browser runs no server, so the browser
# build has none.
if(NOT EMSCRIPTEN)
  # sg serve answers HTTP with cpp-httplib, a library in one header, without
  # HTTPS or compression, and without its own install rules, which would put
  # its header in sg's download.
  set(HTTPLIB_INSTALL OFF)
  set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF)
  set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF)
  set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF)
  set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF)
  FetchContent_Declare(
    httplib
    URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.60.1.tar.gz
  )
  FetchContent_MakeAvailable(httplib)
  add_executable(sg src/main.cc src/server.cc)
  target_link_libraries(sg PRIVATE smollergraph httplib::httplib)
  # On Windows, sg's manifest asks for UTF-8, which our names are written in,
  # for its arguments.
  if(WIN32)
    target_sources(sg PRIVATE src/sg.manifest)
  endif()
  # On Linux, sg carries its own C++ standard library, as RHEL 9 has GCC 11's,
  # older than the one sg is built with.
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND PROJECT_IS_TOP_LEVEL)
    target_link_options(sg PRIVATE -static-libstdc++ -static-libgcc)
  endif()
  target_compile_definitions(sg PRIVATE
                             SMOLLERGRAPH_UI="${PROJECT_SOURCE_DIR}/ui")
  if(NOT PROJECT_IS_TOP_LEVEL)
    set_target_properties(sg PROPERTIES EXCLUDE_FROM_ALL ON)
  endif()
endif()

# The rest is for working on smollerGraph itself: the browser build, the
# tests, the tools and the docs.
if(NOT PROJECT_IS_TOP_LEVEL)
  return()
endif()
enable_testing()

# In a browser, built with emcmake: the library as WebAssembly, and the page's
# way into it, sg.js, with the facts it builds its graph from, in
# build-web/explorer, beside the UI's page, which runs there on its own. A
# browser runs no server, and the tests here are GoogleTest's, so neither is
# built; the module's own test runs it in Node, as a page would load it.
if(EMSCRIPTEN)
  add_executable(sg_web src/web.cc)
  set_target_properties(sg_web PROPERTIES
    OUTPUT_NAME sg
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/explorer)
  target_link_libraries(sg_web PRIVATE smollergraph)
  target_link_options(sg_web PRIVATE
    -lembind -sMODULARIZE -sEXPORT_ES6 -sALLOW_MEMORY_GROWTH
    "SHELL:--preload-file ${PROJECT_SOURCE_DIR}/data/wikidata.txt@/wikidata.txt"
    "SHELL:--preload-file ${PROJECT_SOURCE_DIR}/data/company.txt@/company.txt")
  # The page's files, without the benchmark's, which needs sg serve. CMake
  # copies them again whenever one changes, where a step after the build would
  # only run when sg.wasm is linked again.
  foreach(file index.html page.css app.js view.js api.js)
    configure_file(ui/${file} ${PROJECT_BINARY_DIR}/explorer/${file} COPYONLY)
  endforeach()
  # Emscripten's loader reads sg.data from the folder it runs in, so the test
  # runs in the module's.
  add_test(NAME web COMMAND node ${PROJECT_SOURCE_DIR}/tests/web_test.mjs
                            $<TARGET_FILE:sg_web>
           WORKING_DIRECTORY $<TARGET_FILE_DIR:sg_web>)
  # The explorer's download, its folder in a zip for any web server to serve:
  # cpack --config build-web/CPackConfig.cmake.
  install(DIRECTORY ${PROJECT_BINARY_DIR}/explorer/ DESTINATION .)
  set(CPACK_SYSTEM_NAME web)
  set(CPACK_GENERATOR ZIP)
  include(CPack)
  return()
endif()

# What a download of sg holds, which cmake --install puts in a folder and
# CPack packs in an archive, as cpack --config build/CPackConfig.cmake does:
# sg, the UI's page in a folder beside it, the facts to load, the README and
# the licence.
install(TARGETS sg RUNTIME DESTINATION .)
install(DIRECTORY ui/ DESTINATION ui)
install(DIRECTORY data/ DESTINATION data)
install(FILES README.md LICENSE DESTINATION .)
# Each archive's name ends with the system and the processor it's for, as in
# smollergraph-0.21.0-linux-x86_64.tar.gz, unless it's given another.
if(NOT CPACK_SYSTEM_NAME)
  if(APPLE)
    set(CPACK_SYSTEM_NAME macos-${CMAKE_SYSTEM_PROCESSOR})
  else()
    string(TOLOWER "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
           CPACK_SYSTEM_NAME)
  endif()
endif()
# A tar.gz for Linux, and a zip for Windows and macOS, which open one with a
# double-click.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CPACK_GENERATOR TGZ)
else()
  set(CPACK_GENERATOR ZIP)
endif()
include(CPack)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(googletest)
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
  set(DOXYGEN_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/docs)
  # The README is named by its whole path, as Doxygen 1.9 takes any file named
  # README.md to be the main page, docs/decisions' too, and then leaves that
  # one out.
  set(DOXYGEN_USE_MDFILE_AS_MAINPAGE ${PROJECT_SOURCE_DIR}/README.md)
  # A folder's README.md would be its page under Files, and docs/decisions'
  # is a page of its own.
  set(DOXYGEN_IMPLICIT_DIR_DOCS NO)
  # Each class, function and file links to its code, highlighted, at the line
  # that defines it, and the code shows as it's written, comments and all.
  set(DOXYGEN_SOURCE_BROWSER YES)
  set(DOXYGEN_STRIP_CODE_COMMENTS NO)
  # The menu, with a link to the code on GitHub after the pages.
  set(DOXYGEN_LAYOUT_FILE ${PROJECT_SOURCE_DIR}/docs/doxygen/layout.xml)
  # The README's cover, which docs/doxygen/filter.sh points at here.
  set(DOXYGEN_HTML_EXTRA_FILES ${PROJECT_SOURCE_DIR}/.github/cover.jpg)
  # A record's template is for writing one, so it isn't a page.
  set(DOXYGEN_EXCLUDE ${PROJECT_SOURCE_DIR}/docs/decisions/template.md)
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
  # The Markdown files go through docs/doxygen/filter.sh, which files the
  # decision records under the groups of their index, for the menu.
  set(DOXYGEN_FILTER_PATTERNS
    "*.h=sed -E 's#^ *//#&/#'"
    "*.md=sh ${PROJECT_SOURCE_DIR}/docs/doxygen/filter.sh https://github.com/nenuadrian/smollerGraph"
  )
  # The decision records go in too, a page each, under the README that lists
  # them.
  doxygen_add_docs(docs README.md docs/decisions/README.md docs/decisions
                   include src)
endif()

