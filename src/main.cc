#include <algorithm>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "facts.h"
#include "graph.h"
#include "parser.h"
#include "plan.h"
#include "query.h"
#include "server.h"

namespace {

using Clock = std::chrono::steady_clock;

int Usage() {
  std::cerr
      << "usage: sg load <graph folder> <facts file>\n"
         "       sg walk <graph folder> <start> <relationship type> <hops>\n"
         "       sg bench <graph folder> <starts file> <relationship type> "
         "<hops>\n"
         "       sg serve <graph folder> <port> [address]\n"
         "       sg query <graph folder> <query>\n";
  return 1;
}

// Reads a number of hops, which has to be a whole number that isn't negative.
bool ParseHops(std::string_view text, int& hops) {
  if (absl::SimpleAtoi(text, &hops) && hops >= 0) return true;
  std::cerr << "sg: hops must be a whole number, not " << text << '\n';
  return false;
}

// Adds the facts in the file at `path` to the graph.
int Load(smollergraph::Graph& graph, const char* path) {
  std::ifstream file(path);
  if (!file) {
    std::cerr << "sg: can't open " << path << '\n';
    return 1;
  }
  Clock::time_point start = Clock::now();
  if (absl::Status status = smollergraph::LoadFacts(file, graph);
      !status.ok()) {
    std::cerr << "sg: can't read the fact " << status.message() << '\n';
    return 1;
  }
  // The changed pages are written before the clock stops, so the time
  // includes them.
  graph.Flush();
  std::chrono::duration<double> took = Clock::now() - start;
  std::cout << graph.node_count() << " nodes, " << graph.relationship_count()
            << " relationships, in " << std::fixed << std::setprecision(1)
            << took.count() << " s\n"
            << graph.page_reads() << " pages read, " << graph.page_writes()
            << " written\n";
  return 0;
}

// Prints the nodes `hops` relationships of `type` away from `start`.
int Walk(const smollergraph::Graph& graph, const char* start, const char* type,
         std::string_view text) {
  std::optional<smollergraph::NodeId> node = graph.Find(start);
  if (!node) {
    std::cerr << "sg: there's no node called " << start << '\n';
    return 1;
  }
  int hops = 0;
  if (!ParseHops(text, hops)) return 1;
  for (smollergraph::NodeId found :
       smollergraph::Walk(graph, *node, type, hops)) {
    std::cout << graph.name(found) << '\n';
  }
  return 0;
}

// Times a walk of `hops` relationships of `type` from each node named in the
// file at `path`, after a first pass that warms the system's caches, and
// prints how long opening the graph and the walks took, as JSON.
int Bench(const smollergraph::Graph& graph, const char* path, const char* type,
          std::string_view text, double open_ms) {
  int hops = 0;
  if (!ParseHops(text, hops)) return 1;
  std::ifstream file(path);
  if (!file) {
    std::cerr << "sg: can't open " << path << '\n';
    return 1;
  }
  std::vector<smollergraph::NodeId> starts;
  for (std::string name; std::getline(file, name);) {
    std::optional<smollergraph::NodeId> node = graph.Find(name);
    if (!node) {
      std::cerr << "sg: there's no node called " << name << '\n';
      return 1;
    }
    starts.push_back(*node);
  }
  if (starts.empty()) {
    std::cerr << "sg: " << path << " names no nodes\n";
    return 1;
  }

  size_t results = 0;
  for (smollergraph::NodeId start : starts) {
    results += smollergraph::Walk(graph, start, type, hops).size();
  }
  std::vector<double> micros;
  uint64_t reads_before = graph.page_reads();
  for (smollergraph::NodeId start : starts) {
    Clock::time_point begin = Clock::now();
    smollergraph::Walk(graph, start, type, hops);
    std::chrono::duration<double, std::micro> took = Clock::now() - begin;
    micros.push_back(took.count());
  }
  double reads = static_cast<double>(graph.page_reads() - reads_before);
  std::sort(micros.begin(), micros.end());
  std::cout << std::fixed << std::setprecision(1) << "{\"open_ms\": " << open_ms
            << ", \"queries\": " << micros.size()
            << ", \"results\": " << results
            << ", \"median_us\": " << micros[micros.size() / 2]
            << ", \"p95_us\": " << micros[micros.size() * 95 / 100]
            << ", \"page_reads\": " << reads / micros.size() << "}\n";
  return 0;
}

// Serves the graph over HTTP on the port in `text`, until it's stopped. It
// listens on this machine alone, unless it's given another address.
int Serve(const smollergraph::Graph& graph, std::string_view text,
          const char* address) {
  int port = 0;
  if (!absl::SimpleAtoi(text, &port) || port < 1 || port > 65535) {
    std::cerr << "sg: a port is a number from 1 to 65535, not " << text << '\n';
    return 1;
  }
  if (absl::Status status = smollergraph::Serve(graph, address, port);
      !status.ok()) {
    std::cerr << "sg: " << status.message() << '\n';
    return 1;
  }
  return 0;
}

// Runs a query in our subset of Cypher, and prints what it returns as a table,
// or prints its plan, when it starts with EXPLAIN.
int RunQuery(const smollergraph::Graph& graph, const char* text) {
  Clock::time_point start = Clock::now();
  uint64_t reads = graph.page_reads();
  absl::StatusOr<smollergraph::Query> query = smollergraph::Parse(text);
  absl::StatusOr<smollergraph::Plan> plan =
      query.ok() ? smollergraph::PlanQuery(*query) : query.status();
  if (!plan.ok()) {
    std::cerr << "sg: " << plan.status().message() << '\n';
    return 1;
  }
  if (query->explain) {
    std::cout << smollergraph::Explain(*plan);
    return 0;
  }
  smollergraph::Result result = smollergraph::Run(*plan, graph);
  std::chrono::duration<double, std::milli> took = Clock::now() - start;
  for (size_t i = 0; i < result.columns.size(); ++i) {
    std::cout << (i > 0 ? " | " : "") << result.columns[i];
  }
  std::cout << '\n';
  for (const std::vector<smollergraph::Cell>& row : result.rows) {
    for (size_t i = 0; i < row.size(); ++i) {
      const std::optional<smollergraph::Value>& value = row[i].value;
      std::cout << (i > 0 ? " | " : "")
                << (value ? smollergraph::ValueText(*value) : "null");
    }
    std::cout << '\n';
  }
  // How long the query took, and the pages it read, before the rows were
  // printed.
  uint64_t pages = graph.page_reads() - reads;
  std::cout << result.rows.size()
            << (result.rows.size() == 1 ? " row" : " rows") << " in "
            << std::fixed << std::setprecision(2) << took.count() << " ms, "
            << pages << (pages == 1 ? " page" : " pages") << " read\n";
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  std::string_view command = argc > 1 ? argv[1] : "";
  bool load = command == "load" && argc == 4;
  bool walk = command == "walk" && argc == 6;
  bool bench = command == "bench" && argc == 6;
  bool serve = command == "serve" && (argc == 4 || argc == 5);
  bool query = command == "query" && argc == 4;
  if (!load && !walk && !bench && !serve && !query) return Usage();

  smollergraph::Graph graph;
  Clock::time_point opening = Clock::now();
  if (absl::Status status = graph.Open(argv[2]); !status.ok()) {
    std::cerr << "sg: can't open the graph in " << argv[2] << ": "
              << status.message() << '\n';
    return 1;
  }
  std::chrono::duration<double, std::milli> opened = Clock::now() - opening;
  if (load) return Load(graph, argv[3]);
  if (walk) return Walk(graph, argv[3], argv[4], argv[5]);
  if (serve) return Serve(graph, argv[3], argc == 5 ? argv[4] : "127.0.0.1");
  if (query) return RunQuery(graph, argv[3]);
  return Bench(graph, argv[3], argv[4], argv[5], opened.count());
}
