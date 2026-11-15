# 16\. Draw what a query returns, on a canvas

## Status

Accepted

## Context

Episode 10 loaded the mammals and birds from Wikidata, 21,834 taxa. The page from episode 4
([0010](0010-draw-the-whole-graph-with-svg.md)) asked `sg serve` for the whole graph, about 2.4
million bytes of JSON, and drew each node and relationship as an SVG element, which D3's layout
moved on each tick: a tangle of lines, at a few frames a second.

## Decision

- The page draws what a query returns, "the way Neo4j Browser does", and no more than 500 of its
  nodes (`kMaxDrawn` in `api.h`), with the relationships between them, where Neo4j Browser draws up
  to 300 by default. The rows go in a table, up to 1,000 (`kMaxRows`). `QueryJson` also says how
  long the query took on the server and how many pages it read, which a status line shows.
  `GraphJson` and `WalkJson` and their routes are gone.
- `ui/view.js` draws on a canvas, "one element that the page paints pixels on", in place of an SVG
  element for each circle and line: a path for the relationships, one for their arrowheads, and one
  for each label's nodes, in a colour for each label, each painted with one call however many
  shapes it holds. Names are written while there are 150 nodes or fewer, or once the view is zoomed
  in far enough to read them. D3's force simulation still places the nodes.
- The page is laid out like a console, in `index.html`, `page.css` and `app.js`: a query box with a
  history, Graph, Table and Plan tabs, a search for a node by its name, and the inspector, whose
  button, or a double-click, draws a node's neighbours.

## Alternatives considered

- SVG, as the page drew until then. `ui/bench.html` and `bench/ui.py` (`ep010/step-5`) count the
  frames Chromium paints in three seconds while the page's layout moves a made-up graph, drawn with
  SVG, on a canvas, or not at all. On the run episode 10 recorded, on an Apple M4 Pro, in frames a
  second:

  | Nodes | Not drawn | Canvas | SVG |
  |---|---|---|---|
  | 500 | 59.8 | 59.8 | 59.8 |
  | 1,000 | 60.0 | 59.8 | 59.7 |
  | 2,000 | 60.0 | 59.7 | 59.8 |
  | 5,000 | 59.2 | 59.2 | 23.5 |
  | 10,000 | 29.9 | 27.6 | 11.0 |
  | 20,000 | 13.5 | 11.9 | 4.9 |

- Drawing the whole graph, which "no one can read at this size anyway".

## Consequences

- The canvas stays close to the layout's own pace at each size. Past 5,000 nodes, the layout is
  what's slow, as D3's force simulation runs on the page's one thread. "So the canvas was the
  change worth making, and D3's layout is quick enough for the five hundred nodes a query draws,
  while a whole graph of twenty thousand wouldn't keep up, however it's drawn."
- A canvas has no element for each node to click, so the view finds the node nearest the pointer,
  with D3's `find`, on a press or a click.
- The canvas has as many pixels as the screen, twice its size in CSS pixels on many laptops, so
  circles and text stay sharp.
- Headless Chromium paints a canvas in software, so `bench/ui.py` opens a window.
- A worker could take the layout off the page's thread, for graphs bigger than the 500 nodes drawn;
  not done (episode 10).
