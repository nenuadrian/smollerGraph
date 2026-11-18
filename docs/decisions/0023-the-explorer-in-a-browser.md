# 23\. Run the explorer in a browser, on a graph it builds when it opens

## Status

Accepted

## Context

To try the explorer, someone had to clone the repository, build it, load the Wikidata facts and run
`sg serve`, or run Docker Compose, and a copy anyone could reach would be a server kept up for them
([0014](0014-deploy-to-one-server-behind-caddy.md)). The library is an embedded one
([0002](0002-an-embedded-library-and-sg.md)), without threads, mapped memory or sockets, its files
are plain reads and writes, and its records have fixed sizes, in fields of fixed widths
([0007](0007-fixed-size-records-in-pages-of-4-kib.md)), so Emscripten compiles it as it is (episode
18). The facts are 3.3 MB, and the graph built from them, with its embeddings, is about 29 MB of
files, most of it embeddings; building it from the facts, embeddings and all, takes about a fifth
of a second on the Mac.

## Decision

- Built with `emcmake`, CMake makes `sg.js` and `sg.wasm` from the library and `src/web.cc`, and
  packs `data/wikidata.txt` in `sg.data`. `src/web.cc` gives JavaScript, through embind, the four
  questions `sg serve` answers, as the same JSON from the same functions in `api.h`, and `build`,
  which makes the graph from the facts and gives each node its FastRP embedding along
  `PARENT_TAXON`.
- The page builds the graph each time it opens, in Emscripten's file system, which is in memory, and
  keeps nothing.
- `ui/api.js` asks the graph in the page when `sg.js` is beside it, and `sg serve` over HTTP when it
  isn't, and the status line says which.
- `build-web/explorer` holds the whole explorer, served by any static file server. The Dockerfile's
  `explorer` stage builds it in Emscripten's image, 7.0.0, and runs the module's test in Node, and
  CI publishes it with the API docs, at nenuadrian.github.io/smollerGraph/explorer.

## Alternatives considered

- A server for everyone, with `deploy.sh` behind Caddy ([0014](0014-deploy-to-one-server-behind-caddy.md)):
  a machine to pay for and keep up, for a graph nobody can change.
- Shipping the graph's folder, built on the Mac: about 29 MB to download, where the facts are 3.3 MB.
- Keeping the graph between visits in the browser's IndexedDB, which Emscripten can mount: not
  weighed, as our subset of Cypher can't change a graph, so there's nothing to keep.

## Consequences

- Anyone with a browser can run queries, read a node's record and its bytes, and find the nodes
  most like it, without installing anything, and the page costs nothing to host.
- The page downloads about 3.7 MB, and builds the graph before its first answer, each time it opens.
- It's the one graph, Wikidata's mammals and birds. A graph of your own still needs `sg`.
- The page and `sg serve` share the UI, so a change to the page is a change to both.
  `tests/web_test.mjs` checks the browser build's answers, in Node.
- In the browser, the pages the status line counts are read from memory, not from a disk.
