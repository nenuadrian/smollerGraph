"""Loads the generated friends graph into smollerGraph, Kùzu and Neo4j, asks
each of them two questions in Cypher from each person in starts.txt, three
times over, and writes the medians to bench/results.json: how many people are
two hops of KNOWS away, following them out of the person, and into them.

From the repository's root, after building:

    ./build/make_friends 100000 10 bench/data
    uv run --with kuzu --with neo4j python bench/compare.py

Neo4j runs in Docker.
"""

import json
import shutil
import statistics
import subprocess
import time
from pathlib import Path

import kuzu
from neo4j import GraphDatabase

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "bench" / "data"
NEO4J = "neo4j:5.26-community"
# Each database is loaded and timed this many times, and the median of each
# measure is kept, so one unlucky run doesn't decide the result.
RUNS = 3

# The two questions, the same pattern followed out of a person and into them.
# Our Cypher has no count yet, so smollerGraph returns each of the people once,
# and the rows are counted.
QUESTIONS = {
    "out": "MATCH (a:Person {name: $name})-[:KNOWS]->(:Person)-[:KNOWS]->"
           "(c:Person) ",
    "in": "MATCH (c:Person)-[:KNOWS]->(:Person)-[:KNOWS]->"
          "(a:Person {name: $name}) ",
}
COUNT = "RETURN count(DISTINCT c) AS n"
ROWS = "RETURN DISTINCT c.name"


def timed(ask, starts):
    """Asks for each start once to warm the caches, then again timed, and
    returns the total of the answers and the median and 95th percentile."""
    results = sum(ask(name) for name in starts)
    micros = []
    for name in starts:
        begin = time.perf_counter()
        ask(name)
        micros.append((time.perf_counter() - begin) * 1e6)
    micros.sort()
    return {
        "results": results,
        "median_us": round(micros[len(micros) // 2], 1),
        "p95_us": round(micros[len(micros) * 95 // 100], 1),
    }


def remove(path):
    if path.is_dir():
        shutil.rmtree(path)
    elif path.exists():
        path.unlink()


def smollergraph(starts):
    graph = DATA / "friends.sg"
    remove(graph)
    sg = ROOT / "build" / "sg"
    begin = time.perf_counter()
    subprocess.run([sg, "load", graph, DATA / "friends.txt"], check=True,
                   stdout=subprocess.DEVNULL)
    load_s = time.perf_counter() - begin
    measured = {}
    for question, match in QUESTIONS.items():
        bench = subprocess.run(
            [sg, "bench", graph, DATA / "starts.txt", match + ROWS],
            check=True, capture_output=True, text=True)
        answer = json.loads(bench.stdout)
        measured[question] = {measure: answer[measure]
                              for measure in ("results", "median_us", "p95_us")}
    return {"load_s": round(load_s, 2), **measured}


def kuzu_bench(starts):
    path = DATA / "friends.kuzu"
    remove(path)
    remove(DATA / "friends.kuzu.wal")
    db = kuzu.Database(str(path))
    conn = kuzu.Connection(db)
    conn.execute("CREATE NODE TABLE Person(name STRING PRIMARY KEY)")
    conn.execute("CREATE REL TABLE KNOWS(FROM Person TO Person)")
    begin = time.perf_counter()
    conn.execute(f"COPY Person FROM '{DATA / 'people.csv'}' (header=false)")
    conn.execute(f"COPY KNOWS FROM '{DATA / 'knows.csv'}' (header=false)")
    load_s = time.perf_counter() - begin
    measured = {
        question: timed(lambda name: conn.execute(
            match + COUNT, {"name": name}).get_next()[0], starts)
        for question, match in QUESTIONS.items()}
    return {"load_s": round(load_s, 2), **measured}


def neo4j_bench(starts):
    def docker(*args):
        subprocess.run(["docker", *args], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    subprocess.run(["docker", "rm", "-f", "sg-bench-neo4j"], capture_output=True)
    subprocess.run(["docker", "volume", "rm", "-f", "sg-bench-neo4j"],
                   capture_output=True)
    volumes = ["-v", f"{DATA}:/import", "-v", f"{ROOT / 'bench' / 'neo4j'}:/headers",
               "-v", "sg-bench-neo4j:/data"]
    begin = time.perf_counter()
    docker("run", "--rm", *volumes, NEO4J, "neo4j-admin", "database", "import",
           "full", "--nodes=Person=/headers/people.csv,/import/people.csv",
           "--relationships=KNOWS=/headers/knows.csv,/import/knows.csv")
    load_s = time.perf_counter() - begin

    docker("run", "-d", "--name", "sg-bench-neo4j", "-e", "NEO4J_AUTH=none",
           "-p", "7687:7687", "-v", "sg-bench-neo4j:/data", NEO4J)
    try:
        driver = GraphDatabase.driver("bolt://localhost:7687")
        for _ in range(120):
            try:
                driver.verify_connectivity()
                break
            except Exception:
                time.sleep(1)
        with driver.session() as session:
            # Like Kùzu's primary key, an index finds the start by its name.
            session.run("CREATE INDEX person_name IF NOT EXISTS "
                        "FOR (p:Person) ON (p.name)").consume()
            session.run("CALL db.awaitIndexes()").consume()
            measured = {
                question: timed(lambda name: session.run(
                    match + COUNT, name=name).single()["n"], starts)
                for question, match in QUESTIONS.items()}
        driver.close()
    finally:
        subprocess.run(["docker", "rm", "-f", "sg-bench-neo4j"],
                       capture_output=True)
    return {"load_s": round(load_s, 2), **measured}


def main():
    starts = (DATA / "starts.txt").read_text().splitlines()
    results = {}
    for key, name, bench in [("smollergraph", "smollerGraph", smollergraph),
                             ("kuzu", "Kùzu", kuzu_bench),
                             ("neo4j", "Neo4j", neo4j_bench)]:
        runs = [bench(starts) for _ in range(RUNS)]
        r = results[key] = {"load_s": statistics.median(run["load_s"]
                                                        for run in runs)}
        for question in QUESTIONS:
            if len({run[question]["results"] for run in runs}) != 1:
                raise SystemExit(f"{name} answered differently from run to run")
            r[question] = {measure: statistics.median(run[question][measure]
                                                      for run in runs)
                           for measure in runs[0][question]}
        print(f"{name:13} load {r['load_s']:6.2f} s   out: median "
              f"{r['out']['median_us']:7.1f} µs   in: median "
              f"{r['in']['median_us']:7.1f} µs", flush=True)
    for question in QUESTIONS:
        answers = {r[question]["results"] for r in results.values()}
        if len(answers) != 1:
            raise SystemExit(f"the three answers differ: {answers}")
    print(f"All three agree: {results['smollergraph']['out']['results']} "
          f"people two hops out, and {results['smollergraph']['in']['results']}"
          " in")
    (ROOT / "bench" / "results.json").write_text(
        json.dumps(results, indent=2, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    main()
