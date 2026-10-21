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
