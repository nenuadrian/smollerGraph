# smollerGraph

A small graph database, built from scratch in C++ on the YouTube series
[Building smollerGraph](https://smollergraph.nenuadrian.com). Each step of each episode is a tag
in this repository, so you can check out the code at any point in a video.

It keeps a graph in a folder of fixed-size records, in pages of 4 KiB that a buffer pool keeps in
memory, with a B+ tree that finds a node by its name, and each node's and relationship's properties
in a chain of records of their own: it loads facts written as Cypher patterns, one a line, with a
label after a colon and properties in braces, such as `(Rabbit:Animal {legs: 4})`, walks
relationships of one type from a node, and draws the graph in a browser.

## Building and running

You need CMake 3.28 or newer, Ninja and a C++20 compiler.

```bash
cmake -S . -B build -G Ninja
cmake --build build
./build/sg load animals.sg data/animals.txt
./build/sg walk animals.sg "Red fox" EATS 2
./build/sg query animals.sg "MATCH (a {name: 'Red fox'})-[:EATS*2]->(c) RETURN c.name"
```

`sg query` answers a small subset of [Cypher](https://opencypher.org): a pattern of nodes, with a
label and properties to match, joined by relationships of one type that go right, for one hop or
`*N`, then an optional `WHERE` of comparisons joined by `AND`, such as `b.weight_g < 1000`, and
`RETURN [DISTINCT]` properties of its variables. `EXPLAIN` in front shows the plan.

## Mammals and birds from Wikidata

`data/wikidata.txt` holds 21,834 taxa from [Wikidata](https://www.wikidata.org), which is under
CC0: each species of mammal and bird that the IUCN has assessed, with its scientific name, its
conservation status, its weight when Wikidata has one, and the genera, families, orders and classes
above it, joined by `PARENT_TAXON`. `tools/wikidata.py` writes it from Wikidata's query service, in
a few minutes:

```bash
python3 tools/wikidata.py data/wikidata.txt
./build/sg load wikidata.sg data/wikidata.txt
./build/sg query wikidata.sg "MATCH (s:Species {name: 'Red fox'})-[:PARENT_TAXON*3]->(o) RETURN o.name"
```

## The UI

`sg serve` serves a graph over HTTP, with a page that runs Cypher queries and draws what they
return on a canvas, laid out by [D3](https://d3js.org)'s force simulation, up to 500 nodes at a
time, with the rows in a table, the plan, and what the query cost on the server. A search finds a
node by its name, clicking a node shows its record as it's kept on disk, with its chains of
properties and relationships, and double-clicking it draws the nodes its relationships go to:

```bash
./build/sg serve wikidata.sg 8474
```

Then open http://localhost:8474. Or, with Docker, `docker compose up` loads the mammals and birds
and serves them there.

## Deploying

`deploy.sh` copies the committed code to a server over SSH, and builds and runs it there with
Docker Compose. Caddy, in a container of its own, serves the page's files on port 8474 and passes
`/api/` requests on to `sg serve`, which isn't published:

```bash
./deploy.sh debian@203.0.113.7
```

The server needs Docker, with Compose. The UI answers anyone who can reach the port, and it reads
the graph, so there's nothing to change through it.

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
