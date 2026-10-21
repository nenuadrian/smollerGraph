# smollerGraph

A small graph database, built from scratch in C++ on the YouTube series
[Building smollerGraph](https://smollergraph.nenuadrian.com). Each step of each episode is a tag
in this repository, so you can check out the code at any point in a video.

It keeps a graph in a folder of fixed-size records, in pages of 4 KiB: it loads facts written as
Cypher patterns, one a line, and walks relationships of one type from a node.

## Building and running

You need CMake 3.28 or newer, Ninja and a C++20 compiler.

```bash
cmake -S . -B build -G Ninja
cmake --build build
./build/sg load animals.sg data/animals.txt
./build/sg walk animals.sg "Red fox" EATS 2
```

## Testing

```bash
ctest --test-dir build --output-on-failure
```

## Benchmarks

`bench/compare.py` loads a generated social graph into smollerGraph, Kùzu and Neo4j (in Docker),
and times a two-hop walk of KNOWS from a thousand people in each, three times over, writing the
medians to `bench/results.json`:

```bash
./build/make_friends 100000 10 bench/data
uv run --with kuzu --with neo4j python bench/compare.py
```

## With Docker

The same build and tests on a clean Ubuntu, as CI runs them on each push:

```bash
docker build --target test .
```

## Licence

MIT, see [LICENSE](LICENSE).
