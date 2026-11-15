# 12\. Find a node by its name with a B+ tree

## Status

Accepted

## Context

From episode 1, a graph found a node by its name in a hash map in memory, and from episode 2, with
the graph on disk, opening it rebuilt the map by reading every node record. On the generated graph
of 100,000 people, opening it took 10.4 ms in episode 6: "ten times the nodes, ten times the wait",
and the map took memory for each name, whether it was asked for or not.

## Decision

- `NameIndex` (`name_index.h`, episode 6) is a B+ tree in a file of its own, `names`, through a
  buffer pool of its own of up to 4,096 pages. Page 0 says which page is the root and how many pages
  there are. A page is a 16-byte header, whether it's a leaf, how many entries it has and, in a leaf,
  the next leaf, then its entries in name order. A leaf's entry is a name padded with zeros and the
  id kept for it; above the leaves, an entry is a name and the page with the names from it on.
- `Find` goes down from the root, with a binary search on each level. `Insert` goes down the same
  way, and splits a full page, keeping its first half and moving the rest to a new page on its
  right; above the leaves, the middle entry moves up, and a root that splits gets a new root above
  its two halves.
- Each leaf keeps the page of the leaf after it, so `NamesStartingWith` reads names in order from
  where a prefix would go, leaf to leaf. `GET /api/names` answers with up to eight of them, which
  the page suggests as a name is typed.
- Opening a graph reads the index's header, and no records.

## Alternatives considered

- The hash map in memory, which this replaced. A lookup in it is a single lookup in memory, and it
  "can't tell you which names start with 'Ra', as it keeps them in no order".

## Consequences

- Opening the generated graph went from 10.4 ms to 0.7 ms, "and the gap grows with the graph", and
  the graph no longer keeps each name in memory, just the pages of the index it's using.
- Loading got slower. Each relationship looks up two names, two million lookups for the million
  relationships, and each is a page on each of the tree's three levels, a binary search in each,
  and a copy of each page out of its frame. On the generated million relationships, a load went
  from 0.6 s to 1.4 s, as `sg load` timed itself. "That's a trade most databases make, and Neo4j's
  indexes are B+ trees too."
- An entry took 32 bytes, 127 to a page, and three levels hold over a million names. The index
  keeps names as long as a node's, so from episode 10 an entry took 48 bytes, 85 to a page, and
  from episode 11, 52 bytes, 78 to a page ([0007](0007-fixed-size-records-in-pages-of-4-kib.md)).
- Names are the one thing indexed. A query that starts from a label or a property scans each node,
  where an index would go straight to them (episodes 9 to 11).
- The names come in order, which is what let the page suggest them, "and what range queries in
  Cypher are going to need later" (episode 6).
