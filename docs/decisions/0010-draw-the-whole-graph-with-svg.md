# 10\. Draw the whole graph in the browser, with SVG

## Status

Superseded by [0016](0016-draw-what-a-query-returns-on-a-canvas.md)

## Context

Episode 1's plan gave smollerGraph a browser UI that "draws query results as a graph with D3, the
way Neo4j Browser does". Episode 4 built the first one, before the database had a query language,
on a graph of ten animals, "a bit like Neo4j Browser". Why it drew the whole graph, and why with
SVG, isn't recorded.

## Decision

- `sg serve` answers `GET /api/graph` with the whole graph, `GraphJson`: each node with its id and
  name, and each relationship with its id, its two nodes and its type (`ep004/step-3`).
- `ui/index.html` loads D3 7.9 from jsDelivr, and `ui/graph.js` draws the graph in an SVG element:
  a group with a circle and a name for each node, and a group with a line, an arrowhead and a type
  for each relationship (`ep004/step-5`).
- D3's force simulation places them: each relationship pulls its nodes towards 170 pixels apart,
  each node pushes the others away, with a strength of -900, nodes collide 60 pixels apart, and the
  graph is pulled to the middle. On each tick, every line and every node's group is moved.
- A walk from a node lights up each hop's nodes a second apart (`GET /api/walk`, `ep004/step-6`),
  and clicking a node opens an inspector with its record's page, offset and bytes, and its chain
  (`GET /api/node/<id>`, `ep004/step-7`).
- `sg serve` serves the page's files from the source tree, which CMake passes in as
  `SMOLLERGRAPH_UI`, so a change to the page needs a reload, not a build.

## Alternatives considered

Not recorded for this version. [0016](0016-draw-what-a-query-returns-on-a-canvas.md) lists what was
measured when it was replaced.

## Consequences

- Drawing the graph took about seventy lines of JavaScript at `ep004/step-5`.
- On the mammals and birds from Wikidata, 21,834 nodes, the page asked for about 2.4 million bytes
  of JSON, made an SVG element for each node and relationship, and moved each of them on each tick:
  "a tangle of lines, with circles flying apart, at a few frames a second" (episode 10).
- Measured in episode 10, in Chromium, SVG kept up with the screen to 2,000 nodes, and fell to 23.5
  frames a second at 5,000 and 4.9 at 20,000, as the browser lays out and paints an element for
  each circle and line on each frame.
- Episode 10 replaced the page. `GraphJson`, `WalkJson` and their routes went, as a query does what
  the walk form did, and the inspector and the node's route stayed
  ([0016](0016-draw-what-a-query-returns-on-a-canvas.md)).
- The page's files are still served from the source tree, and on a server Caddy serves them
  ([0014](0014-deploy-to-one-server-behind-caddy.md)).
