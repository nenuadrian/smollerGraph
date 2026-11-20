FROM ubuntu:24.04 AS toolchain
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build ca-certificates \
 && rm -rf /var/lib/apt/lists/*

FROM toolchain AS build
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build

FROM build AS test
RUN ctest --test-dir build --output-on-failure

FROM python:3.13-slim AS lint
RUN pip install --no-cache-dir clang-format==19.1.7 cpplint==2.0.2
WORKDIR /lint
COPY CPPLINT.cfg .clang-format ./
COPY include include
COPY src src
COPY tests tests
COPY tools tools
COPY ui ui
RUN find include src tests tools ui -name '*.cc' -o -name '*.h' -o -name '*.js' \
 | xargs clang-format --dry-run --Werror \
 && cpplint --recursive --quiet include src tests tools

FROM toolchain AS tidy
RUN apt-get update \
 && apt-get install -y --no-install-recommends clang-18 clang-tidy-18 \
 && rm -rf /var/lib/apt/lists/*
ENV PATH=/usr/lib/llvm-18/bin:$PATH CXX=clang++
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja \
 && run-clang-tidy -quiet -p build '^/src/(src|tools)/'

# The API docs as CI publishes them: docker build --target docs --output site .
# writes their pages to site/. Configuring is enough, as building the docs
# reads the sources and compiles nothing.
FROM toolchain AS docs-build
RUN apt-get update \
 && apt-get install -y --no-install-recommends doxygen graphviz \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja \
 && cmake --build build --target docs

# The explorer as it runs in a browser: the library built as WebAssembly by
# Emscripten, in its own image, beside the page's files, with the module's
# test run in Node.
FROM emscripten/emsdk:7.0.0 AS explorer
RUN apt-get update \
 && apt-get install -y --no-install-recommends ninja-build \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN emcmake cmake -S . -B build-web -G Ninja \
 && cmake --build build-web \
 && ctest --test-dir build-web --output-on-failure

# The docs, and the explorer at explorer/, so GitHub Pages serves both.
FROM scratch AS docs
COPY --from=docs-build /src/build/docs/html /
COPY --from=explorer /src/build-web/explorer /explorer
