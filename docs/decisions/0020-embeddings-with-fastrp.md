# 20\. Embeddings with FastRP, kept in the graph's folder

## Status

Accepted

## Context

An embedding turns a node's place in the graph into a vector, so that nodes with similar
neighbourhoods get vectors that point in similar directions, and finding the nodes most like one is
a comparison of vectors (episode 14). Neo4j does this in its Graph Data Science library, on a copy
of the graph it projects into memory, in several ways: FastRP, which mixes random vectors along the
relationships and needs no training, node2vec, which learns vectors from random walks, and
GraphSAGE, which trains a small neural network.

## Decision

- FastRP (`embed.h`), "as it's the simplest": a very sparse random vector of 256 numbers for each
  node, from a seed of 42, then five rounds in which a node's vector is the sum of its neighbours'
  from the round before, scaled to a length of one, and the embedding adds up the rounds from the
  second on. GDS's default is three rounds, and a tree of taxa needs two more, as a species'
  relatives are four hops away (`ep014/step-1`).
- `Cosine` compares two embeddings, and `MostSimilar` keeps the k nodes most like one, comparing it
  with each of the others.
- `sg embed` computes them once, and keeps them in the graph's folder, in `embeddings`, a record of a
  kibibyte for each node whose number is the node's id, four to a page. `sg similar` reads them back
  (`ep014/step-2`), and `Embeddings` gives none unless each node has one.

## Alternatives considered

- Computing them each time `sg similar` runs, as `ep014/step-1` did, which is quick on 22,000 nodes,
  "and wasted work even so, as the vectors don't change until the graph does".
- Writing each embedding back as a property of its node, as GDS can. "Our properties hold a number
  or a string, so a file of their own suits ours better."
- node2vec and GraphSAGE. Episode 15 trains a GraphSAGE, outside the database
  ([0021](0021-machine-learning-outside-the-database.md)).

## Consequences

- On the Wikidata graph, the lion's nearest are the other species of Panthera, at a cosine of one,
  then Panthera and the cat family, from nothing more than the relationships (episode 14).
- `sg embed` took about a tenth of a second for the 22,000 nodes, and wrote about 5,500 pages. The
  file is 22 MB, nearly three times the rest of the graph, as "the size of a vector is a trade
  between how far apart random vectors stay, and the room they take".
- Embeddings belong to the graph as it was when `sg embed` ran: a graph that has gained nodes since
  has none to give until it runs again.
- `MostSimilar` compares a node with each of the others, where a vector index, such as Neo4j's,
  finds close vectors without looking at each, and our FastRP knows the shape of the graph and
  nothing else, where GDS's can start from a node's properties (episode 14).
- The database's container runs `sg embed` on the Wikidata graph before it serves, and the page's
  inspector draws a node's ten nearest, each line longer as the cosine falls.
