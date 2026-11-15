# 17\. Labels and types as tokens, and each relationship in two chains

## Status

Accepted

## Context

A node kept the relationships that start at it ([0003](0003-relationships-beside-their-nodes.md)),
so a pattern could be followed only the way its arrows point. In episode 11, with the Wikidata graph
loaded, a question from mvGPT, the GPT from another series on this channel, for the critically
endangered species of Manidae, the pangolins, was planned as a label scan of about seventeen
thousand species and a walk two hops up from each critically endangered one, and read more than
1,300 pages, though the family is one node the index finds in a few. On the generated friends,
asking who's two hops of KNOWS into a person scanned each person, took about a second and read over
nine thousand pages, to find 78 people.

A second chain needs four bytes more in each record, and neither had them. A node's 64 bytes were a
44-byte name, a 12-byte label and two ids, and a relationship's 32 a 16-byte type, its two nodes,
its next and its first property. And both spent their bytes on names many records share: about
seventeen thousand taxa had the label Species, and each relationship the type PARENT_TAXON.

## Decision

- Labels and relationship types are tokens, as in Neo4j. A `TokenStore` (`token_store.h`) keeps
  each name once, in a file of 32-byte records whose ids are the tokens, `labels` or `types`, and in
  memory both ways, as there are few. A record keeps the four-byte token in place of the name. A
  query looks names up with `Find`, which adds none.
- Each relationship is in two chains, as Neo4j's are: one out of the node it starts at, and one
  into the node it ends at. A node's record is a 48-byte name, its label's token, its first
  relationship out, its first in and its first property, still 64 bytes. A relationship's is its
  type's token, its two nodes, its next out, its next in and its first property, 24 bytes, a
  quarter less, 170 to a page.
- `AddRelationship` puts a relationship at the front of both chains, and `Neighbours` follows
  either, by a `Direction`, comparing tokens in place of names; from episode 12, both at once.

## Alternatives considered

- Kùzu's way, its compressed sparse row adjacency kept twice, once in each direction, so a walk
  backwards reads the backward copy, sorted by the node relationships end at (episode 11). "Our
  relationships are in chains already, so we're going to stay close to Neo4j."
- Neo4j's links to the previous relationship in each chain, and its groups by type and direction
  for a node with a lot of relationships. Episode 11 described both; it gave no reason for leaving
  them out.
- Padding a relationship's record to 32 bytes, as 24 doesn't divide 4,096 and a page has sixteen
  bytes left at its end: "that would waste eight bytes in each of them, where this wastes sixteen in
  a page" (episode 11).
- A hash map from names to tokens, which would hash a name in place of the sorted map's few
  comparisons, "but with a handful of names, the difference is too small to measure".

## Consequences

- The pangolins' plan starts from a seek for Manidae and follows its chain in: 33 pages instead of
  more than 1,300, in about forty-five times less time ([0013](0013-a-subset-of-cypher.md)). Into a
  person on the friends, the median was 36.6 µs, with parsing and planning, where a scan had taken
  about a second (episode 11).
- A chain still holds a node's relationships of each type in one direction, so a walk along one
  type steps over the others, which Neo4j's groups avoid (episode 11).
- A label or a type can be up to 31 bytes, and a node's name 47.
- Nothing removes a token, as nothing removes a label or a type from a graph yet.
- Opening a graph reads its tokens into memory, a page or two for a graph with a few labels and
  types.
