# 1\. C++20, in Google's style, built with CMake and Ninja

## Status

Accepted

## Context

Episode 1 looked at Neo4j, which is written in Java, and Kùzu, which is written in C++, and then
at what smollerGraph should be. It said the database is going to be written in C++, "which is what
Kùzu is written in, and what we used for nanoMMO", the MMO from another series on this channel. No
episode weighs another language, so this record says what came with the choice rather than why it
won.

## Decision

- The database is C++20, built with CMake, 3.28 or newer, and Ninja (`ep001/step-1`). The standard
  is required, so a compiler that can't do C++20 stops with an error instead of falling back to an
  older one, and the compilers' extensions are off. Episode 1 named designated initialisers and
  `starts_with` as what C++20 gives us.
- `-Wall -Wextra -Wpedantic` have been on since the first commit, "because they get more expensive
  to fix the longer we wait".
- The code follows Google's C++ style guide: `.cc` and `.h` files, header guards named after the
  path, data members ending in an underscore, and a `.clang-format` based on Google's style, so
  nobody formats anything by hand. From episode 4, cpplint checks the guide and clang-tidy checks
  for bugs, as errors, in CI ([0006](0006-build-test-and-check-in-docker.md)).
- The tests use GoogleTest 1.18.0, which CMake fetches when it configures, and
  `gtest_discover_tests` registers each test with CTest (`ep001/step-5`). Abseil and cpp-httplib
  come the same way, each pinned to a release.
- The tool that generates the social graph for the benchmarks, `tools/make_friends.cc`, is C++
  too (episode 3). The scripts around the database are Python, run with uv where they need
  packages: `bench/compare.py` (episode 3), `tools/wikidata.py` and `bench/ui.py` (episode 10), and
  `tools/gnn.py` (episode 15).

## Alternatives considered

- Java, as Neo4j is written in, or any other language. These are the obvious alternatives, and
  none was weighed here.

## Consequences

- The guide doesn't use exceptions, so errors come back as values: a `bool` and a string at first,
  and `absl::Status` from episode 4 ([0009](0009-errors-as-absl-status.md)).
- The guide doesn't allow `<filesystem>`, which it says lacks support for testing and has security
  problems. Until episode 4 the graph made its folder with `std::filesystem`, with an error code,
  as the guide doesn't use the exceptions it throws otherwise. From `ep004/step-1`, the graph and
  `make_friends` make their folders with `mkdir`, and the tests get a new folder from `mkdtemp`.
- `CPPLINT.cfg` leaves out the copyright check, as the licence is in `LICENSE`, and the check for
  includes without a folder, as the sources are all in `src` and include each other by name.
- With Clang, CMake looks for C++20 modules, which we don't use, and adds flags clang-tidy can't
  read, so `CMAKE_CXX_SCAN_FOR_MODULES` is off (episode 4).
- A compiler can work out a call's arguments in any order, and GCC and Clang don't always agree,
  so `LoadFacts` adds a fact's two nodes in statements of their own, which gives them ids in the
  order they're named on any compiler (episode 2).
- `export` is a word C++ keeps for modules, so the flag for `sg export` in `main.cc` is called
  `to_folder` (episode 15).
- There were four tests at the end of episode 1, and 67 at the end of episode 15, each built and
  run on each push.
