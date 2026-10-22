#!/usr/bin/env bash
# Builds smollerGraph on a server and serves the animals there with Docker
# Compose, on port 8474: ./deploy.sh debian@203.0.113.7
set -euo pipefail
server=$1

# The committed files of HEAD, as a tar stream, unpacked into ~/smollergraph on
# the server.
git archive HEAD | ssh "$server" \
  'rm -rf smollergraph && mkdir smollergraph && tar -x -C smollergraph'
# Build the image there, and swap the running container for one from it.
ssh "$server" 'cd smollergraph && docker compose up -d --build --wait'
