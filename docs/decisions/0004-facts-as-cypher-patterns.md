# 4\. Load a graph from facts written as Cypher patterns, one a line

## Status

Accepted

## Context

Episode 1 needed a way to get its ten animals into the throwaway graph, without building them by
hand in `main`. Every graph the series has used since came in the same way: the animals, a social
graph generated for the benchmarks (episode 3), the mammals and birds on Wikidata (episode 10), and
a model's predictions (episode 15).

## Decision

- A graph comes from a file of facts, one a line, written as a Cypher pattern, such as
  `(Red fox)-[:EATS]->(Rabbit)`, which episode 1 said "keeps the file easy to read, and close to how
  we'd ask for them in Cypher". Blank lines and lines starting with `#` are skipped. From episode 9
  a node can be a fact on its own, and nodes and relationships can have properties in braces, a
  string or a whole number; from episode 10 a node can have a label after a colon, as in
  `(Rabbit:Animal {legs: 4})`.
- `LoadFacts` reads a stream, not a file name, so a test can hand it a string. It stops at the
  first line it can't read, and returns that line as the error's message. A name is a node: adding
  a name the graph has gives back its node. A name, type, label, key or string too long for its
  record is refused before anything on its line is added.
- `sg load` adds a facts file to the graph in a folder (episode 2), and it's how a graph is written,
  with its embeddings written by `sg embed` ([0020](0020-embeddings-with-fastrp.md)). Cypher, the
  page and the API read.
- Programs write facts: `tools/make_friends.cc`, the generated social graph, with CSV files beside
  it for Neo4j and Kùzu (episode 3); `tools/wikidata.py`, `data/wikidata.txt`, 21,834 taxa from
  Wikidata's query service, whose data is under CC0, "so it's free to use, and to keep in our
  repository" (episode 10); and `tools/gnn.py`, its predictions (episode 15).

## Alternatives considered

- CSV files and a bulk loader, as episode 3 loads the generated graph into Neo4j, with
  `neo4j-admin`'s import, and into Kùzu, with `COPY`. `make_friends` writes those files for them,
  and they weren't weighed as smollerGraph's own way in.
- Cypher's `CREATE` and `SET`, which write through the query language. This is the obvious
  alternative, and it wasn't weighed: properties come from facts, "as our Cypher can't set them
  yet" (episode 9).

## Consequences

- A load goes a line at a time, through `AddNode` and `AddRelationship`. In episode 3 it took
  12.63 s for the generated million relationships, built without optimizations, against 0.63 s for
  Kùzu and 7.93 s for Neo4j, which read CSV files on several cores and write their files in large
  pieces. Optimized, it took 12.03 s, and the buffer pool took that to 1.07 s (episode 5,
  [0011](0011-a-buffer-pool-of-our-own.md)).
- The facts spell out each name in full, so the generated million relationships take about 40 MB
  (episode 3).
- A name is a node, so two taxa with one English name would be one node. `wikidata.py` gives each
  taxon the first name that fits and isn't taken: its English name, its scientific name, or its
  Wikidata id (episode 10).
- Our lexer doesn't read escapes, so `wikidata.py` puts a value with an apostrophe in double quotes
  (episode 10).
- A model's predictions are facts, which `sg load` adds to a graph without touching anything else
  (episode 15).
- Writing through `sg load` and `sg embed` alone keeps the page safe to leave open on a server
  ([0014](0014-deploy-to-one-server-behind-caddy.md)). A graph that's written to needs to survive a
  crash part way through a write, which is what a write-ahead log is for, and the episodes up to 16
  leave it for later.
