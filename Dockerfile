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
# cpplint names each header guard after its path, so the sources go in a
# folder named after the project, for guards such as SMOLLERGRAPH_GRAPH_H_.
WORKDIR /lint
COPY CPPLINT.cfg .clang-format ./
COPY src smollergraph
COPY tests tests
COPY tools tools
COPY ui ui
RUN find smollergraph tests tools ui -name '*.cc' -o -name '*.h' -o -name '*.js' \
 | xargs clang-format --dry-run --Werror \
 && cpplint --root=. --recursive --quiet smollergraph tests tools

FROM toolchain AS tidy
RUN apt-get update \
 && apt-get install -y --no-install-recommends clang-18 clang-tidy-18 \
 && rm -rf /var/lib/apt/lists/*
ENV PATH=/usr/lib/llvm-18/bin:$PATH CXX=clang++
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja \
 && run-clang-tidy -quiet -p build '^/src/(src|tools)/'
