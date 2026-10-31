"""Trains a small graph neural network on a graph that sg export wrote: a
GraphSAGE, whose layers each give a node a new vector from its own and the
average of its neighbours'. It learns which species are threatened, from their
weights and from what's known about their relatives in the tree of taxa, and
writes its predictions as facts, which sg load adds to the graph.

From the repository's root, after building:

    ./build/sg export wikidata.sg export
    uv run --with torch --with numpy python tools/gnn.py export predictions.txt
    ./build/sg load wikidata.sg predictions.txt
"""

import csv
import json
import math
import sys
from pathlib import Path

import torch

# The IUCN's statuses: the three it counts as threatened, and two that aren't.
# A species listed as data deficient, or not assessed, has neither.
THREATENED = {"vulnerable", "endangered", "critically endangered"}
NOT_THREATENED = {"least concern", "near threatened"}
# Four layers reach four hops, from a species through its genus to its family,
# and back down to the family's other species.
LAYERS = 4
HIDDEN = 32
EPOCHS = 300


def load(folder):
    """Reads the nodes, and the relationships as two lists of node ids, the
    starts and the ends."""
    with open(folder / "nodes.jsonl") as f:
        nodes = [json.loads(line) for line in f]
    with open(folder / "relationships.csv") as f:
        rows = list(csv.DictReader(f))
    return nodes, [int(r["from"]) for r in rows], [int(r["to"]) for r in rows]


class GraphSage(torch.nn.Module):
    """Layers that each give each node a new vector, from its own vector and
    the average of its neighbours', and an output that turns the last vector
    into a score."""

    def __init__(self, features, layers, hidden):
        super().__init__()
        sizes = [features] + [hidden] * layers
        self.layers = torch.nn.ModuleList(
            torch.nn.Linear(2 * a, b) for a, b in zip(sizes, sizes[1:]))
        self.out = torch.nn.Linear(hidden, 1)

    def forward(self, x, senders, receivers, degree):
        for layer in self.layers:
            # Each node adds up the vectors its neighbours send it, and
            # divides by how many there are.
            total = torch.zeros_like(x).index_add_(0, receivers, x[senders])
            x = torch.relu(layer(torch.cat([x, total / degree], dim=1)))
        return self.out(x).squeeze(1)


def auc(scores, labels):
    """The area under the ROC curve: the chance that a threatened species
    scores higher than one that isn't, 0.5 for a guess and 1 for no mistakes."""
    order = sorted(range(len(scores)), key=lambda i: scores[i])
    rank_sum = sum(rank for rank, i in enumerate(order, 1) if labels[i])
    positives = sum(labels)
    negatives = len(labels) - positives
    return (rank_sum - positives * (positives + 1) / 2) / (positives * negatives)


def main():
    folder, out = Path(sys.argv[1]), Path(sys.argv[2])
    nodes, starts, ends = load(folder)
    n = len(nodes)
    # Messages go both ways along each relationship, from parent to child and
    # back, so each relationship is a sender and a receiver twice.
    senders = torch.tensor(starts + ends)
    receivers = torch.tensor(ends + starts)
    degree = torch.zeros(n).index_add_(
        0, receivers, torch.ones(len(receivers))).clamp(min=1)[:, None]

    # The species with a status that says whether they're threatened, and
    # the answer for each, 1 for threatened.
    status = [v["properties"].get("status") for v in nodes]
    species = [i for i in range(n) if status[i] in THREATENED | NOT_THREATENED]
    ids = torch.tensor(species)
    answer = torch.tensor([float(status[i] in THREATENED) for i in species])
    print(f"{n} nodes, {len(species)} species with a status, "
          f"{int(answer.sum())} of them threatened")

    # One thread, as with more, the order a sum is added up in can change from
    # run to run, and floats round differently in each order, so each run would
    # train a slightly different network.
    torch.set_num_threads(1)

    # A random 70% to learn from, 15% to pick the best epoch with, and 15% to
    # test on, which no choice is made with.
    torch.manual_seed(0)
    order = torch.randperm(len(species))
    train = order[:len(species) * 70 // 100]
    valid = order[len(species) * 70 // 100:len(species) * 85 // 100]
    test = order[len(species) * 85 // 100:]

    # Each node's own features: its weight, on a log scale, as weights run
    # from grams to tonnes, and whether it has one at all.
    base = torch.zeros(n, 2)
    for v in nodes:
        grams = v["properties"].get("weight_g")
        if grams:
            base[v["id"]] = torch.tensor([math.log10(grams + 1) / 6, 1.0])

    def features(known):
        """The base features, and two more, which say whether a species is
        threatened or not, for the species in `known`, and nothing for any
        other node."""
        label = torch.zeros(n, 2)
        label[ids[known], 0] = answer[known]
        label[ids[known], 1] = 1 - answer[known]
        return torch.cat([base, label], dim=1)

    model = GraphSage(4, LAYERS, HIDDEN)
    optimiser = torch.optim.Adam(model.parameters(), lr=0.01)
    best_auc, best_state = 0.0, None
    for epoch in range(1, EPOCHS + 1):
        # Half of the training species show their answers, and the network
        # learns to predict the other half, so it can't copy a species' own.
        shuffled = train[torch.randperm(len(train))]
        shown = shuffled[:len(shuffled) // 2]
        hidden = shuffled[len(shuffled) // 2:]
        scores = model(features(shown), senders, receivers, degree)
        loss = torch.nn.functional.binary_cross_entropy_with_logits(
            scores[ids[hidden]], answer[hidden])
        optimiser.zero_grad()
        loss.backward()
        optimiser.step()
        # Each epoch is scored on the validation species, with each training
        # answer shown, and the best epoch's weights are kept.
        with torch.no_grad():
            scores = model(features(train), senders, receivers, degree)[ids]
        valid_auc = auc(scores[valid].tolist(), answer[valid].tolist())
        if valid_auc > best_auc:
            best_auc = valid_auc
            best_state = {k: t.clone() for k, t in model.state_dict().items()}
        if epoch % 50 == 0:
            print(f"epoch {epoch:3}  loss {loss.item():.3f}  "
                  f"validation AUC {valid_auc:.3f}")
    model.load_state_dict(best_state)

    # A rule to compare with, written by hand: the share of threatened species
    # among the training species of a species' genus, and of its family, each
    # with two made-up species at the overall share, so that a small genus
    # doesn't decide alone.
    parent = dict(zip(starts, ends))
    prior = answer[train].mean().item()
    def share(group):
        threatened, total = {}, {}
        for j in train.tolist():
            g = group(species[j])
            threatened[g] = threatened.get(g, 0) + answer[j].item()
            total[g] = total.get(g, 0) + 1
        return [(threatened.get(group(i), 0) + 2 * prior) /
                (total.get(group(i), 0) + 2) for i in species]
    genus = share(lambda i: parent.get(i))
    family = share(lambda i: parent.get(parent.get(i)))
    rule = [(g + f) / 2 for g, f in zip(genus, family)]

    labels = answer[test].tolist()
    with torch.no_grad():
        scores = model(features(train), senders, receivers, degree)
    print(f"The rule, on the test species:     AUC "
          f"{auc([rule[j] for j in test.tolist()], labels):.3f}")
    print(f"The network, on the test species:  AUC "
          f"{auc(scores[ids[test]].tolist(), labels):.3f}")

    # The risk of each species the network didn't learn from, from 0 to 100,
    # as facts: a node's name, and the property in braces.
    learnt = set(ids[train].tolist())
    risk = torch.sigmoid(scores) * 100
    written = 0
    with open(out, "w") as f:
        for v in nodes:
            if v["label"] == "Species" and v["id"] not in learnt:
                f.write(f"({v['name']} {{risk: {round(risk[v['id']].item())}}})\n")
                written += 1
    print(f"The risk of {written} species, in {out}")


if __name__ == "__main__":
    main()
