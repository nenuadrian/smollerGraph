"""Writes mammals and birds from Wikidata as facts for sg load: each species
the IUCN has assessed, with its genus, family, order and class, its scientific
name, its conservation status, its weight when Wikidata has one, and its
Wikidata id. Wikidata's data is under CC0, so the facts can go in the repo.

From the repository's root:

    python3 tools/wikidata.py data/wikidata.txt

It asks Wikidata's query service a few hundred small questions, which takes a
few minutes.
"""

import json
import sys
import time
import urllib.parse
import urllib.request

ENDPOINT = "https://query.wikidata.org/sparql"
AGENT = "smollerGraph (https://smollergraph.nenuadrian.com)"
# The two classes, Mammalia and Aves, and the ranks we keep, by their ids.
CLASSES = ["Q7377", "Q5113"]
RANKS = {
    "Q37517": "Class",
    "Q36602": "Order",
    "Q35409": "Family",
    "Q34740": "Genus",
    "Q7432": "Species",
}
SPECIES = "Q7432"
# How many taxa go in one question: more than this, and some answers take
# longer than the query service allows.
BATCH = 300
# What fits in smollerGraph's records: a name of 43 bytes, a string of 42.
MAX_NAME = 43
MAX_STRING = 42


def ask(query):
    """Runs a SPARQL query, waiting and trying again when the service is busy,
    and returns its rows."""
    data = urllib.parse.urlencode({"query": query, "format": "json"}).encode()
    request = urllib.request.Request(ENDPOINT, data, {"User-Agent": AGENT})
    for attempt in range(5):
        try:
            with urllib.request.urlopen(request, timeout=90) as response:
                return json.load(response)["results"]["bindings"]
        except OSError:
            time.sleep(10 * (attempt + 1))
    sys.exit("wikidata.py: the query service didn't answer")


def item(row, key):
    """The id at the end of an entity's address, such as Q7377."""
    return row[key]["value"].rsplit("/", 1)[1]


def walk(root):
    """Walks the taxonomy down from a class a level at a time, as our Walk
    does, and returns each taxon's rank and parent taxon. A species is as far
    down as it goes: its subspecies are left out."""
    taxa = {root: ("Q37517", None)}
    frontier = [root]
    while frontier:
        reached = []
        for i in range(0, len(frontier), BATCH):
            values = " ".join("wd:" + q for q in frontier[i : i + BATCH])
            for row in ask(
                f"SELECT ?child ?parent ?rank WHERE {{ VALUES ?parent "
                f"{{ {values} }} ?child wdt:P171 ?parent . "
                f"OPTIONAL {{ ?child wdt:P105 ?rank }} }}"
            ):
                child = item(row, "child")
                if child in taxa:
                    continue
                rank = item(row, "rank") if "rank" in row else None
                taxa[child] = (rank, item(row, "parent"))
                if rank != SPECIES:
                    reached.append(child)
        frontier = reached
    return taxa


def describe(ids):
    """Returns each taxon's English name, British where Wikidata has one,
    its scientific name, its IUCN status and the masses Wikidata gives for it,
    in kilograms, leaving out a mass with another role than an adult's, such
    as a weight at birth."""
    found = {q: {"label": None, "taxon": None, "status": None, "kg": []}
             for q in ids}
    for i in range(0, len(ids), BATCH):
        values = " ".join("wd:" + q for q in ids[i : i + BATCH])
        for row in ask(f"""
SELECT ?taxon ?british ?label ?name ?status ?kg WHERE {{
  VALUES ?taxon {{ {values} }}
  OPTIONAL {{ ?taxon rdfs:label ?british .
             FILTER(LANG(?british) = "en-gb") }}
  OPTIONAL {{ ?taxon rdfs:label ?label . FILTER(LANG(?label) = "en") }}
  OPTIONAL {{ ?taxon wdt:P225 ?name }}
  OPTIONAL {{ ?taxon wdt:P141/rdfs:label ?status .
             FILTER(LANG(?status) = "en") }}
  OPTIONAL {{ ?taxon p:P2067 ?mass .
             ?mass psn:P2067/wikibase:quantityAmount ?kg .
             FILTER NOT EXISTS {{ ?mass pq:P3831 ?role .
                                 FILTER(?role != wd:Q78101716) }} }}
}}"""):
            taxon = found[item(row, "taxon")]
            for key, column in (("label", "british"), ("label", "label"),
                                ("taxon", "name")):
                if column in row and taxon[key] is None:
                    taxon[key] = row[column]["value"]
            if "status" in row and taxon["status"] is None:
                taxon["status"] = row["status"]["value"].lower()
            if "kg" in row:
                taxon["kg"].append(float(row["kg"]["value"]))
    return found


def keep(taxa, found):
    """The assessed species, and the ranked taxa above them, each with the
    nearest of those above it as its parent."""
    kept = {}
    for q, (rank, _) in taxa.items():
        if rank != SPECIES or found[q]["status"] is None:
            continue
        child = q
        parent = taxa[q][1]
        while parent is not None:
            if taxa[parent][0] in RANKS:
                kept[child] = parent
                child = parent
            parent = taxa[parent][1]
        kept.setdefault(child, None)
    return kept


def quoted(text):
    """A string as Cypher writes it, in whichever quotes it doesn't hold."""
    return f'"{text}"' if "'" in text else f"'{text}'"


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: python3 tools/wikidata.py <facts file>")
    nodes = []  # (id, rank, parent, what describe found)
    for root in CLASSES:
        taxa = walk(root)
        species = [q for q, (rank, _) in taxa.items() if rank == SPECIES]
        found = describe(species)
        kept = keep(taxa, found)
        found.update(describe([q for q in kept if q not in found]))
        print(f"{root}: {len(taxa)} taxa, {len(kept)} kept", file=sys.stderr)
        nodes += [(q, RANKS[taxa[q][0]], parent, found[q])
                  for q, parent in kept.items()]

    # Classes first, then orders and so on down to species, so a node's
    # parent is in the file before it. A species goes by its English name, as
    # we would say it, and the ranks above by their scientific names.
    order = list(RANKS.values())
    nodes.sort(key=lambda node: (order.index(node[1]), node[0]))
    names = {}
    taken = set()
    for q, rank, _, taxon in nodes:
        label = taxon["label"] or ""
        name = (label[:1].upper() + label[1:] if rank == "Species" else "")
        for choice in (name, taxon["taxon"], q):
            # Two taxa with one name would be one node, so the second takes its
            # scientific name, or its id.
            if (choice and len(choice.encode()) <= MAX_NAME
                    and not set(choice) & set("()[]{}:")
                    and choice not in taken):
                names[q] = choice
                taken.add(choice)
                break

    with open(sys.argv[1], "w", encoding="utf-8") as out:
        out.write(
            "# Mammals and birds from Wikidata, which is under CC0, written\n"
            "# by tools/wikidata.py: each species the IUCN has assessed, and\n"
            "# the genera, families, orders and classes above it.\n")
        for q, rank, _, taxon in nodes:
            properties = []
            if taxon["taxon"] and len(taxon["taxon"].encode()) <= MAX_STRING:
                properties.append("taxon: " + quoted(taxon["taxon"]))
            if taxon["status"]:
                properties.append("status: " + quoted(taxon["status"]))
            if taxon["kg"]:
                # The mean of the masses it has, in grams, as a whole number.
                grams = sum(taxon["kg"]) / len(taxon["kg"]) * 1000
                properties.append(f"weight_g: {max(1, round(grams))}")
            properties.append("wikidata: " + quoted(q))
            out.write(f"({names[q]}:{rank} {{{', '.join(properties)}}})\n")
        for q, _, parent, _ in nodes:
            if parent is not None:
                out.write(f"({names[q]})-[:PARENT_TAXON]->({names[parent]})\n")
    print(f"{len(nodes)} taxa written to {sys.argv[1]}", file=sys.stderr)


if __name__ == "__main__":
    main()
