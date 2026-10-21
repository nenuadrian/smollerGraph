# smollerGraph

A small graph database, built from scratch in C++ on the YouTube series
[Building smollerGraph](https://smollergraph.nenuadrian.com). Each step of each episode is a tag
in this repository, so you can check out the code at any point in a video.

For now, it's a graph kept in memory: it loads facts written as Cypher patterns, one a line, and
walks relationships of one type from a node.

## Building and running

You need CMake 3.28 or newer, Ninja and a C++20 compiler.

```bash
cmake -S . -B build -G Ninja
cmake --build build
./build/sg data/animals.txt "Red fox" EATS 2
```

## Testing

```bash
ctest --test-dir build --output-on-failure
```

## Licence

MIT, see [LICENSE](LICENSE).
