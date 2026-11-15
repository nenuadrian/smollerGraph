# 19\. Search for paths from both ends, and write them as Neo4j and Kùzu do

## Status

Accepted

## Context

A shortest path answers how to get from one node to another in as few hops as possible, and for
the animals, how closely two species are related (episode 12). Neo4j wraps a pattern in
`shortestPath`, and its manual says it runs "a fast bidirectional breadth-first search"; Kùzu writes
it inside the relationship, as a star, SHORTEST and a range of hops. A cheapest path, whose weights
add up to the least, is what version two of LDBC's interactive workload asks for between two
people, each friendship weighed by the messages the two exchanged, and what its paper says
Dijkstra's algorithm solves (episode 13).

## Decision

- `ShortestPath` runs a breadth-first search from each end, the end's side following relationships
  the other way. Each round, the side with fewer nodes to go on from takes a hop, until a side
  reaches a node the other has reached, and as each side takes a whole hop at a time, the first
  meeting gives a shortest path (`ep012/step-2`).
- `CheapestPath` runs Dijkstra's algorithm from each end, with a priority queue, each round settling
  a node on the side whose next one is cheaper. When a side lowers the cost of a node the other has
  reached, that's a way from end to end, and the cheapest is kept. It stops once the two sides' next
  costs add up to that cheapest way, "as a path through nodes neither has settled can't cost less"
  (`ep013/step-3`). A relationship whose weight isn't a whole number of zero or more isn't followed.
- In Cypher, a shortest path is written as Neo4j writes it, `MATCH p = shortestPath((a)-[:T*]-(b))`,
  and a cheapest path as Kùzu does, `-[:T* WSHORTEST(weight)]-`, as Neo4j's Cypher has none. Our
  relationships have no variables, so `cost(p)` takes the path's, next to `length(p)`. The plan
  finds both ends, each the way that's cheapest for it, then runs the search, "which is what Neo4j's
  plan does too".
- `make_friends` gives each friendship LDBC's weight, `max(round(40 - sqrt(messages)), 1)`, for a
  made-up number of messages from 1 to 1,600, from a generator of its own, seeded with 7, so the
  people each person knows don't change (episode 13).

## Alternatives considered

- A search from one end, which each episode wrote first, and measured.
- For a cheapest path in Neo4j, a procedure from the Graph Data Science library, which copies the
  graph into memory first, or from APOC (episode 13).

## Consequences

- On the generated friends, a shortest path of five hops between two people reached about 98,000 of
  the 100,000 from one end, reading all 7,446 pages in about 40 ms, and about 1,500 from both ends,
  reading about 800 pages in about a millisecond (episode 12). A cheapest path of ten hops at a cost
  of 16 reached all 100,000 from one end, reading about 23,000 pages in about 350 ms, and about 4,800
  from both, reading about 4,800 pages in about 6 ms. On 150 pairs, the two searches agreed on each
  cost (episode 13).
- From both ends, a search for a path that isn't there costs more. The red fox and the tawny owl
  have none, so both sides go through the mammals and the birds, about 22,000 taxa, before one runs
  out, where a search from one end would have stopped at the mammals (episode 12).
- Left out: all shortest paths, a limit on the number of hops (`*..N`), A*, which needs a guess of
  the cost left, and negative weights, which need Bellman-Ford (episodes 12 and 13). A star with no
  number is refused outside a shortest or a cheapest path.
- A weight is a property, so each hop of a cheapest path reads one more record
  ([0015](0015-properties-in-chains-of-records.md)).
