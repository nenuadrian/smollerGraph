# 2\. An embedded library, with sg as its program

## Status

Accepted

## Context

Episode 1 compared two ways a graph database runs. Neo4j is a server that applications connect to
over the network, through Bolt, its own binary protocol, or over HTTP. Kùzu is embedded: there's no
server, and it runs inside a program as a library, the way SQLite and DuckDB do. Weighing what
smollerGraph should be, episode 1 said: "My instinct right now is an embedded database, like Kùzu,
so it's a library you can open from a test, with nothing to start first." It wasn't called settled
then, and the code has been built that way since.

## Decision

- The database is a library, `smollergraph`, built from `src/`: the graph and its files, the
  facts, the queries, the JSON answers, the embeddings and the export. A `Graph` opened without a
  folder lives in temporary files with no name, which go when it closes (episode 2).
- `sg` is a program that links the library, with a command for each thing a person does with a
  graph: `load`, `walk`, `path`, `embed`, `similar`, `export`, `bench`, `serve` and `query`.
- HTTP lives in `sg`, not in the library. `sg serve` (episode 4) answers with the JSON that
  `api.cc`, in the library, writes, kept apart from HTTP "so the tests can check it without a
  server". It uses cpp-httplib 0.60.1, a library in one header, without HTTPS or compression, as
  "an HTTP server is a project of its own, and not the one this series is about".
- `sg serve` listens on 127.0.0.1 unless it's given another address, as it has no passwords, "so
  that's the safer default".
- Programs in other languages reach a graph through `sg` and through files: `bench/compare.py` runs
  `sg load` and `sg bench` as processes (episode 3), and `tools/gnn.py` reads what `sg export`
  writes, and hands back facts for `sg load` ([0021](0021-machine-learning-outside-the-database.md)).

## Alternatives considered

- A server, like Neo4j, which applications connect to with a protocol of its own. Episode 1 put
  the two side by side and leaned the other way, for the reason above.
- A Python package with the whole database inside it, as Kùzu has (episode 1). This is an obvious
  next step for an embedded database, and it wasn't weighed.

## Consequences

- The tests open graphs directly, with nothing to start, and the JSON the page gets is tested
  without a server (episode 4).
- The library and the tests don't link cpp-httplib, and with OpenSSL, zlib, Brotli and zstd turned
  off in `CMakeLists.txt`, the build doesn't depend on what happens to be installed.
- cpp-httplib lets several servers share a port by default, so a second `sg serve` on a busy port
  would start without a word, and the two would take turns answering. `server.cc` sets
  `SO_REUSEADDR` alone, and a busy port is refused (episode 4).
- `sg serve` answers requests on several threads at once, so the buffer pool needed a latch
  ([0011](0011-a-buffer-pool-of-our-own.md)).
- Nothing over HTTP changes the graph, which is written by `sg load` alone, and its embeddings by
  `sg embed` ([0004](0004-facts-as-cypher-patterns.md), [0020](0020-embeddings-with-fastrp.md)). On
  a server, `sg serve` sits behind Caddy, which is all that's published
  ([0014](0014-deploy-to-one-server-behind-caddy.md)).
- Inside a container, the loopback address is the container's own, so `compose.yaml` passes
  `0.0.0.0` (episode 4).
