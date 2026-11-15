# 18\. Build vanilla versions of what Neo4j and Kùzu offer, and stop racing them

## Status

Accepted

## Context

Episodes 3, 5 and 11 measured smollerGraph against Neo4j and Kùzu
([0008](0008-measure-against-neo4j-and-kuzu.md)). Made fair in episode 11, the comparison still set
a function inside `sg` against a server over its network protocol in a container and a library
called from Python, "so we're not claiming to be faster than either". From episode 12 the series
turned to what those two offer and smollerGraph didn't: shortest and cheapest paths, embeddings,
and a graph neural network, several of which Neo4j offers in its Graph Data Science library.

## Decision

- Episodes 12 to 15 run no comparison against Neo4j or Kùzu, and each calls its feature a vanilla
  version of theirs. Episode 12: "this is a vanilla version of what Neo4j and Kùzu offer. Their
  searches are built for graphs far bigger than ours, with years of engineering behind them, while
  ours is there to show how a shortest path works." Episodes 13, 14 and 15 say the same of
  cheapest paths, FastRP and the GraphSAGE.
- A feature is measured against our own simpler version of it, not against theirs: a search from
  one end against a search from both (episodes 12 and 13), and a graph neural network against a
  rule written by hand (episode 15).
- What we take from them is named: Neo4j's `shortestPath`, Kùzu's `WSHORTEST`, and GDS's FastRP and
  GraphSAGE.

## Alternatives considered

- Going on measuring against Neo4j and Kùzu, as episodes 3, 5 and 11 did
  ([0008](0008-measure-against-neo4j-and-kuzu.md)). No episode weighs it out loud.

## Consequences

- The numbers in episodes 12 to 15 are ours against ours: from both ends, a shortest path reached
  about 1,500 people where one end reached 98,000 (episode 12), and the network's AUC was 0.734
  against the rule's 0.729 (episode 15).
- Each episode ends its features with what its version leaves out, next to what theirs do: all
  shortest paths, A*, a vector index, sampling a node's neighbours in batches on a big graph.
- `bench/compare.py` and `sg bench` are as episode 11 left them, and nothing has been added to them
  since.
- Each episode says plainly that its version isn't going to scale the way theirs does.
