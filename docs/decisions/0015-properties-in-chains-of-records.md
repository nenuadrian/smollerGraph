# 15\. Properties in chains of records of their own

## Status

Accepted

## Context

Until episode 9, a node had a name and a relationship a type, and nothing else. Episode 9 gave both
properties, and went back to the two ways episode 1 had shown. Neo4j's record format gives a node
the id of its first property record, and each property record points at the next, so a node's
properties form a chain, like its relationships. Kùzu keeps each property in a column of its own,
"so a query that compares weights reads the weights, and skips the rest".

## Decision

- We follow Neo4j, as "our nodes and relationships are records already, which point at their
  chains". A third file of records, `properties`, holds a key, the type of the value, the value,
  and the id of the next property. A node's record and a relationship's hold the id of their first
  property (`ep009/step-1`).
- A value is a whole number or a string. `SetProperty` puts a property at the front of its chain,
  or replaces the value of a key the chain has; `FindProperty` follows the chain, and `Properties`
  lists them in the order they were first set.
- A node's name stays in its record. `FindProperty` gives it as the property `name`, which a fact
  can't use as a key.
- From episode 13, a relationship's property can be looked up by key, with
  `FindRelationshipProperty`, through the loop it shares with `FindProperty`, `Lookup`, which is how
  a cheapest path reads each friendship's weight ([0019](0019-paths-from-both-ends.md)).

## Alternatives considered

- Kùzu's way, a column for each property, which episode 9 weighed.
- Long strings in records of their own, chained from the property, as Neo4j keeps them. Episode 9
  lists it among what's left out.

## Consequences

- The property's id took four bytes from a node's name and from a relationship's type, so both
  stayed 32 bytes, and a string held 14 bytes. From episode 10 a property takes 64 bytes, with a key
  of up to 15 bytes and a string of up to 42 ([0007](0007-fixed-size-records-in-pages-of-4-kib.md)).
- A graph without properties walks as it did: on the generated million relationships, which have
  none, a two-hop walk's median was 6.5 µs before and 6.0 µs after (episode 9).
- Reading a property follows a chain, a record a read. A cheapest path reads each weight through
  its relationship's chain, "where a weight kept next to the relationship would save a read for each
  hop"; from one end, it read about 23,000 pages, the properties file among them (episode 13).
- A value has no decimals (episode 9), and the risk `tools/gnn.py` gives a species is written as a
  whole number from 0 to 100 (episode 15).
- A missing property is null, which keeps no rows, as in Cypher, and values of different types are
  never equal and have no order (episode 9).
- Only names are indexed, so a query that starts from a property scans each node
  ([0012](0012-a-b-plus-tree-for-names.md)), and properties come from facts, as Cypher can't set
  them ([0004](0004-facts-as-cypher-patterns.md)).
