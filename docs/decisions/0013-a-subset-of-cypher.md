# 13\. Our own subset of Cypher, planned with Neo4j's operators

## Status

Accepted

## Context

Neo4j and Kùzu both speak Cypher, which draws the pattern a query looks for in ASCII art, and
episode 1 said smollerGraph "should speak a small subset of Cypher, enough for the queries we care
about". Until episode 7, `sg` answered one kind of question, a walk of one type from a node.

## Decision

- A query goes from text to answers in four stages, "which is how most databases do it": `Lex`
  splits it into tokens, `Parse`, a recursive descent parser with a function for each part of the
  grammar, reads them into a `Query`, `PlanQuery` turns that into steps, and `Run` runs them
  (episode 7). Keywords are read in any case, and an error says what was expected, where, and what
  was there.
- The subset grows as the database can answer it: `MATCH`, a pattern of nodes and relationships of
  one type, `*N` hops, `RETURN [DISTINCT]` and `EXPLAIN` (episode 7); `WHERE`, properties in
  patterns, and any property in `RETURN` (episode 9); labels (episode 10); arrows to the left
  (episode 11); paths ([0019](0019-paths-from-both-ends.md)).
- The plan's steps take Neo4j's operator names, `NodeIndexSeek`, `AllNodesScan`,
  `NodeByLabelScan`, `Expand` and `Filter`, "so that EXPLAIN in smollerGraph reads a bit like
  Neo4j's".
- A filter goes in as soon as its node has a slot, so the rows it drops are expanded no further,
  which real planners call pushing a predicate down (episode 9).
- A plan starts from the node that's cheapest to find, by a rule: one with a name, through the
  index, then one with a label, then any node, the first of them on a tie. It expands from there
  to the pattern's right end, then to its left end (episode 11).
- `Run` runs each step on all of the rows before the next one starts.

## Alternatives considered

- Picking the start from statistics on the graph, such as how many nodes have each label, as Neo4j
  and Kùzu estimate what each start costs (episode 11). Ours follows "a simpler rule".
- Passing rows from step to step one at a time, or in small batches, as real databases do. Episode
  7 left it for later.

## Consequences

- With the rule, a question from mvGPT, the GPT from another series on this channel, for the
  critically endangered species of Manidae, the pangolins, starts from a seek for the family and
  follows its chain in: 33 pages where a label scan of the species read more than 1,300, in about
  forty-five times less time (episode 11).
- Each step holds all of its rows in memory, "which a bigger graph would run out of memory doing"
  (episode 7).
- The expands don't check that a path uses each relationship once, as Cypher does, "which matters
  once the graph has cycles" (episode 7).
- A label scan reads each node's record, as there's no index on labels (episode 10). And the rule
  goes by what the pattern says, where Neo4j and Kùzu "estimate how many rows each start gives,
  from statistics on the graph" (episode 11).
- Our Cypher has no parameters, so `sg bench` puts each start's name in quotes in place of `$name`,
  and no `count`, so it returns each person once (episode 11).
- Cypher reads. It can't create or set anything; a graph is written by `sg load`, and its
  embeddings by `sg embed` ([0004](0004-facts-as-cypher-patterns.md),
  [0020](0020-embeddings-with-fastrp.md)).
- `sg query` and the page's query box both run it, and the page shows `EXPLAIN`'s plan in a tab of
  its own.
