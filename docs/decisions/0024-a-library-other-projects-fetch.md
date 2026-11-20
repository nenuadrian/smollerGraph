# 24\. A library that other projects fetch, at a version

## Status

Accepted

## Context

smollerGraph was built to be used on its own, through `sg`, and to be worked on, with its tests,
tools and docs ([0002](0002-an-embedded-library-and-sg.md)). Another project that fetched it with
CMake's FetchContent, as it fetches Abseil, got all of that (episode 20, `tests/consumer`): a build
of 178 steps with our tests and `sg` in it, our 71 tests in its `ctest`, GoogleTest, cpp-httplib
and the docs' theme downloaded, the build type and the C++ standard set for it, and `SMOLLERGRAPH_UI`
pointing at a folder of its own, as `CMAKE_SOURCE_DIR` is the top-level project's. Our headers had
names such as `graph.h`, which could clash with that project's. A game, nanoMMO, wants to check its
content as a graph with the library.

## Decision

- Fetched by another project, the build adds the library, `smollergraph::smollergraph`, and `sg`,
  which that project builds by naming it. The tests, the tools, the browser build and the docs are
  added when smollerGraph is the top-level project (`PROJECT_IS_TOP_LEVEL`), and so are the default
  build type and the C++ standard. The library asks for C++20 at least, with
  `target_compile_features`, which a project that links it can raise.
- Paths come from `PROJECT_SOURCE_DIR` and `PROJECT_BINARY_DIR`.
- The headers are in `include/smollergraph`, and included as `"smollergraph/graph.h"`, inside the
  library too.
- `Execute` runs a query in one call, for a project that wants answers, not plans.
- `tests/consumer` fetches the library and asks it a question, and CI builds it on the code in hand,
  with `FETCHCONTENT_SOURCE_DIR_SMOLLERGRAPH`.
- From episode 20, each release has a tag, `v0.20.0` for episode 20, and `project()` has its
  version, so another project pins a version, not an episode's step.

## Alternatives considered

- Copying the library's sources into each project that uses it: no download, but a copy to keep in
  step by hand, and nothing to say which version it is.
- An installed package, with `install(EXPORT)` and a config file for `find_package`: the usual way
  for a library a system installs once. Not weighed beyond that, as the projects in view fetch
  their libraries at a version, and a package would be a second way in to keep working.
- A repository of its own for the library, apart from the database's tools: more to keep in step,
  for a library this small.

## Consequences

- The headers moved, so code that included `"graph.h"` includes `"smollergraph/graph.h"`.
- smollerGraph declares Abseil as `abseil`, so a project that declared it first shares its copy,
  which has to have what we use.
- Each release needs its version tag, which follows the episode, and says nothing yet about
  whether a release can break the code that uses it.
- CI builds Abseil a second time, for the consumer.
