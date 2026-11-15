# 8\. Measure each change against Neo4j and Kùzu

## Status

Superseded by [0018](0018-vanilla-versions-not-races.md)

## Context

Episode 1's plan was to measure smollerGraph along the way, "running the queries we run on Neo4j
and Kùzu", and to benchmark it against them on LDBC's Social Network Benchmark, "at a scale that
fits on a laptop". Episode 3 measured it for the first time, and said that from then on "each
change comes with these charts, before and after".

## Decision

- `tools/make_friends.cc` generates a social graph: people who each know a number of others, picked
  at random from `mt19937`'s raw output with a fixed seed, 42. The standard defines those numbers
  to the bit, so each run on each machine writes the same graph, where the distributions differ
  between standard libraries. Nobody is their own friend, so a two-hop walk never uses one
  relationship twice, which Neo4j's patterns don't allow. 100,000 people with 10 friends each is a
  million relationships.
- `sg bench` times a walk from each of a thousand starts, once to warm the caches and once timed,
  and prints the median and the 95th percentile as JSON.
- `bench/compare.py` loads the graph into smollerGraph, into Kùzu in process, and into Neo4j 5.26
  in Docker, asks each the same question for the same thousand people, how many people are two hops
  of KNOWS away, and does it all three times, keeping the median of each measure, "so one unlucky
  run doesn't decide the result". The answers have to agree, from run to run and across the three.

## Alternatives considered

- LDBC's Social Network Benchmark itself, which episode 1's plan named. Episode 3 wanted "a graph we
  can make again on any machine", and wrote a generator of its own; whether LDBC's own data was
  weighed isn't recorded.

## Consequences

- Episode 3's medians, on the generated million relationships: smollerGraph loaded them in 12.63 s,
  Neo4j in 7.93 s and Kùzu in 0.63 s; a two-hop walk took 70.1 µs in smollerGraph, 747.1 µs in Neo4j
  and 1,545.7 µs in Kùzu. Episode 3 called the walk "not a fair fight yet": ours was a function call
  written for one question, where the other two parsed, planned and ran Cypher, through Python, and
  Neo4j over Bolt to a server in a container.
- That `sg` was built without optimizations, which episode 5 corrected, measuring the before again:
  12.03 s and 46.3 µs. With the buffer pool, 1.07 s and 5.8 µs
  ([0011](0011-a-buffer-pool-of-our-own.md)).
- To make it fair, episode 11 had `sg bench` parse, plan and run Cypher, and asked two questions,
  following KNOWS out of a person and into them. The medians: smollerGraph 28.3 µs out and 36.6 µs
  in, Neo4j 883.8 and 660.0, Kùzu 1,607.6 and 1,825.0. Even then, "Neo4j's times still include a
  trip over its network protocol to a container, and Kùzu's a call from Python, while ours are
  measured inside sg, so we're not claiming to be faster than either."
- A comparison runs Neo4j's import and its server in Docker, three times over, and takes minutes,
  several in episode 3 and a couple in episode 11, with nothing else busy on the machine.
- From episode 12 the series stopped racing them ([0018](0018-vanilla-versions-not-races.md)).
  `bench/compare.py` and `sg bench` stay as episode 11 left them, and `make_friends` goes on
  writing the graph the later episodes search ([0019](0019-paths-from-both-ends.md)).
