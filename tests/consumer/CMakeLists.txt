# Another project, which uses smollerGraph as a library: it fetches it, as it
# would fetch Abseil, and links it. CI builds it on this checkout, with
# -DFETCHCONTENT_SOURCE_DIR_SMOLLERGRAPH=<the checkout>, in place of the
# download, so it tries the code in hand.
cmake_minimum_required(VERSION 3.28)
project(consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  smollergraph
  URL https://github.com/nenuadrian/smollerGraph/archive/refs/tags/v0.20.0.tar.gz
)
FetchContent_MakeAvailable(smollergraph)

add_executable(consumer main.cc)
target_link_libraries(consumer PRIVATE smollergraph::smollergraph)

enable_testing()
add_test(NAME consumer COMMAND consumer)
