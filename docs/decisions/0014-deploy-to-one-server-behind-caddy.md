# 14\. Deploy to one server with deploy.sh, behind Caddy

## Status

Accepted

## Context

Up to episode 8, smollerGraph's page ran at localhost, "where nobody else can open it", with
`sg serve` sending the page's files and answering the API, published on port 8474 from a
container. Episode 8 put it on a server.

## Decision

- Episode 8 deployed to a VPS from DreamHost, with two virtual CPUs and 4 GB of memory, "plenty
  for our animals, and for a graph a good deal bigger", which came with Docker installed.
- `deploy.sh` takes the server as a user at an address, such as `./deploy.sh debian@203.0.113.7`.
  It sends the committed files of HEAD over SSH, as a tar stream from `git archive`, into
  `~/smollergraph`, after removing the old copy, and runs `docker compose up -d --build --wait`
  there, which builds the image and swaps the running containers for new ones.
- `compose.yaml` runs two containers, each restarted unless it was stopped, so they come back when
  the server reboots. `web` is Caddy 2.10.2 on Alpine, the only one published, which serves the
  `ui` folder on port 8474 and passes `/api/` on to `sg`. `sg` runs `sg serve` on the network the
  two share. The episode's reason: "On a server, it's usual to split those up", and Caddy serves
  the files "as a web server is built to".
- It's plain HTTP, on port 8474, "as there's no name to get a certificate for yet".

## Alternatives considered

- `sg serve` sending the page and answering the API on the server's port, as it did on a laptop
  from episode 4, and in `ep008/step-1`.

## Consequences

- The database answers the web server alone, which is "the one place to add HTTPS or limits
  later". With a name pointing at the server, Caddy could get a certificate and serve the page over
  HTTPS (episode 8).
- `git archive` packs what's committed, so a change that isn't committed doesn't reach the server,
  and removing the old folder first means a file deleted from the repo doesn't stay behind.
- The image is built on the server, from the toolchain up, which took a couple of minutes on the
  VPS's two cores the first time; Docker keeps each step, so a deploy builds again from the first
  step whose files changed ([0006](0006-build-test-and-check-in-docker.md)).
- On the VPS, Docker published ports past ufw, the server's firewall, so the page could be reached
  as soon as it ran (episode 8).
- The page reads the graph and changes nothing, "which is why it's fine to leave open". Writing
  through it would need someone to be allowed to, and a graph that's written to would need to
  survive a crash in the middle of a write, "which is what a write-ahead log is for"
  ([0004](0004-facts-as-cypher-patterns.md), [0011](0011-a-buffer-pool-of-our-own.md)).
- From episode 10 the database's container loads and serves the mammals and birds from Wikidata,
  from episode 14 it gives them their embeddings first, and `compose.friends.yaml` serves the
  generated friends in their place (episode 13).
- Caddy and `sg serve` send the page's files with `Cache-Control: no-cache`, so a browser asks for
  them again after an upgrade (episode 10).
