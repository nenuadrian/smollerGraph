# 21\. Train models outside the database, through sg export and sg load

## Status

Accepted

## Context

Episode 15 trained a graph neural network on the mammals and birds, to tell which species are
threatened from what's known about their relatives. Neo4j's Graph Data Science library offers
GraphSAGE, and its pipelines train a model to classify nodes inside the database, from their
embeddings and properties, and write each prediction back to its node, as a property. Episode 15
gives no reason for going another way beyond showing how it works: ours is "a vanilla version of
that, a script that trains outside the database, and sg load to write the predictions back".

## Decision

- "A model trains outside the database, in a script, which needs the graph in a form it can read."
  `sg export` writes a graph into a folder (`ep015/step-1`): `nodes.jsonl`, a line of JSON for each
  node with its id, name, label and properties, "so a reader can take the file a line at a time",
  and `relationships.csv`, the ids of the two nodes each relationship joins, and its type.
- `tools/gnn.py` trains a GraphSAGE of four layers in PyTorch, run with uv, as four hops reach a
  species' relatives through its genus and its family. With the label trick, statuses go in as
  features: each epoch, half of the training species show theirs, and the network learns to predict
  the other half from them, so no species sees its own answer. 70% of the species with a status are
  for training, 15% to pick the best epoch, and 15% to test on.
- It trains on one thread, as "with more, the order a sum is added up in can change from run to
  run, and floats round differently in each order, so each run would train a slightly different
  network" (`gnn.py`).
- It writes the risk it gives the species it didn't learn from as facts, which `sg load` adds to the
  graph ([0004](0004-facts-as-cypher-patterns.md)).

## Alternatives considered

- Training inside the database, as GDS does. Not weighed: it comes up in episode 15's list of what
  ours leaves out, where Neo4j's library "trains inside the database, and writes back from there
  too, where ours goes out to a script and back in through sg load".

## Consequences

- On the test species, the network's AUC was 0.734, and a rule written by hand, the share of
  threatened species in a species' genus and family, scored 0.729. "The honest reading is a modest
  one": what the network learnt is close to what the rule says, that relatives share their risk.
  Training took about ten seconds, on one thread (episode 15).
- The predictions are facts like any other, so Cypher asks about them: fifteen species the IUCN
  lists as least concern have a risk over 70.
- The database gained an export and nothing else. PyTorch and NumPy stay out of its build, and
  `uv run --with torch --with numpy` brings them in for the script.
- A type is a single word, so none holds a comma, and the CSV needs no quoting.
- The script averages all of a node's neighbours at once, where GraphSAGE on a big graph averages a
  sample of them, in batches, and the graph has one kind of relationship, where a knowledge graph
  has many (episode 15).
