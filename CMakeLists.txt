cmake_minimum_required(VERSION 3.28)
project(smollergraph VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic)

add_library(smollergraph src/graph.cc src/facts.cc src/query.cc)
target_include_directories(smollergraph PUBLIC src)

add_executable(sg src/main.cc)
target_link_libraries(sg PRIVATE smollergraph)
