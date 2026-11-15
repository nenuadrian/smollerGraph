# 6\. Build, test and check in Docker, on each push

## Status

Accepted

## Context

Episode 1 built smollerGraph on a Mac, with Clang, and ran its tests there. Episode 2 opened with
"a bit of engineering we skipped last time", so that each step from then on is built and tested on
a clean machine, and the build doesn't depend on what's installed on the one it runs on.

## Decision

- One multi-stage `Dockerfile`, from Ubuntu 24.04 (`ep002/step-1`): `toolchain` installs the
  compilers, CMake, Ninja and the certificates FetchContent needs to download over HTTPS, without
  recommended packages; `build` configures and builds in Release; and `test` runs ctest, with
  output on failure, so a failing test stops the build. `.dockerignore` keeps the build folder and
  `.git` out of the image's context.
- GitHub Actions builds the same stages (`.github/workflows/ci.yml`) on each push to main, on each
  pull request and from a button, so "CI builds and tests the code with the commands you'd run on
  your own machine".
- Episode 4 added two checks as stages, which nanoMMO, the MMO from another series on this channel,
  has had from the start: `lint`, clang-format 19.1.7 and cpplint 2.0.2, pinned, in a slim Python
  image, and `tidy`, clang-tidy 18, with its checks for bugs, the static analyzer's, threads and
  performance as errors. CI runs `lint` before the tests and `tidy` after them.
- Episode 16 added `docs-build` and `docs`, which build the API docs
  ([0022](0022-api-docs-from-the-comments.md)).
- `compose.yaml` runs `sg serve` from the build stage (episode 4), and the same file runs on the
  server ([0014](0014-deploy-to-one-server-behind-caddy.md)).

## Alternatives considered

- Building and testing on a developer's machine as it is, which is where episode 1 started.
  Episode 2 moved off it so that each step is built and tested on a clean machine.
- Installing the toolchain on CI's machines, without Docker. This is the obvious alternative, and
  it wasn't weighed.

## Consequences

- On Ubuntu the compiler is GCC, where on the Mac it's Clang, "so the code gets checked by a second
  compiler with its own warnings" (episode 2). The tidy stage reads the code with Clang 18.
- Docker doesn't run a stage it has cached, tests included, so running the tests again takes
  `--no-cache-filter test` (episode 2).
- The first build downloads Ubuntu and the toolchain, then Abseil, GoogleTest and cpp-httplib. On
  the server's two cores that took a couple of minutes, and Docker keeps each step, so the next
  build starts from the first step whose files changed (episode 8).
- Docker's builds have been Release since episode 2, while a build on the host without a build type
  had no optimizations until `ep005/step-1` made Release the default. Episode 3's benchmark ran
  such a build ([0008](0008-measure-against-neo4j-and-kuzu.md)).
- cpplint names each header guard after its path, so the lint stage copies `src` into a folder
  named after the project, for guards such as `SMOLLERGRAPH_GRAPH_H_` (episode 4).
