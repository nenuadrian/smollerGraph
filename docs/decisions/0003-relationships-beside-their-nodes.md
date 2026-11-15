# 3\. Keep each node's relationships beside it, in records that point at each other

## Status

Accepted

## Context

A graph database is built for queries that walk many hops. Episode 1 first pictured its food chain
in a relational database such as MySQL: what the fox's prey eat is a join of a table of what eats
what with a second copy of it, and each hop further is one more join, through an index. Then it
looked at how Neo4j and Kùzu keep a graph. Neo4j has store files of fixed-size records, where a
node's record holds the id of its first relationship, and each relationship's record links to the
next and the previous relationships of both of its nodes, which Neo4j calls index-free adjacency.
Kùzu keeps columns, with its adjacency in compressed sparse row form: a type's relationships sorted
by the node they start at, the neighbours one after another in an array, and a second array saying
where each node's neighbours start.

Episode 1 leaned towards records that point at each other, like Neo4j's, "as it's the simplest way
to see what a hop costs, and maybe columns later, to measure the difference".

## Decision

- Each node keeps its relationships beside it, so a hop follows a stored id, without a search. In
  episode 1's graph in memory, that was a vector of the ids of the relationships that start at a
  node (`ep001/step-2`).
- On disk, as in Neo4j's record format (`ep002/step-4`): a node's record holds the id of its first
  relationship, and each relationship's record holds its type, its two nodes and the id of the next
  relationship from the same node, so a node's relationships form a chain. `kNoRelationship`, the
  largest 32-bit number, ends it.
- A new relationship goes at the front of its node's chain: its next is the node's old first, and
  the node's first becomes the new relationship. That's two writes, however long the chain is. The
  chain is newest first, and `Neighbours` reverses what it reads, so callers get relationships in
  the order they were added, as they did from the vectors.

## Alternatives considered

- Kùzu's columns and compressed sparse row adjacency, where a node's neighbours sit next to each
  other, "which is what disks and CPU caches are fastest at", and a query reads the properties it
  needs and skips the rest (episode 1). Left for later, to measure against; no episode up to 16 has
  built them.
- Tables and a join for each hop, as a relational database would keep the graph, which episode 1
  showed for contrast. Not weighed.
- A link to the previous relationship as well as the next, as Neo4j's records have. Not weighed.

## Consequences

- A hop reads a record for each relationship in the chain. In episode 5, a two-hop walk on the
  generated million relationships read 121 pages: one for each of the eleven nodes whose
  relationships it follows, and one for each of their 110 relationships. The buffer pool took that
  to none ([0011](0011-a-buffer-pool-of-our-own.md)).
- A crash between the two writes of `AddRelationship` leaves a relationship that no chain points
  to (episode 2). That's what transactions and a log are for, and up to episode 16 the database has
  neither.
- A chain held the relationships that start at a node, so a pattern could be followed only the way
  its arrows point, and in episode 10 the species of a family were found by scanning each species
  and walking up. Episode 11 put each relationship in a second chain, of those that end at a node
  ([0017](0017-tokens-and-two-chains.md)).
- A chain holds a node's relationships of every type, so a walk along one type steps over the
  others, which Neo4j avoids, for a node with a lot of relationships, by splitting them into groups
  by type and direction (episode 11).
