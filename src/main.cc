#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "facts.h"
#include "graph.h"
#include "query.h"

namespace {

using Clock = std::chrono::steady_clock;

int Usage() {
  std::cerr
      << "usage: sg load <graph folder> <facts file>\n"
         "       sg walk <graph folder> <start> <relationship type> <hops>\n"
         "       sg bench <graph folder> <starts file> <relationship type> "
         "<hops>\n";
  return 1;
}

// Reads a number of hops, which has to be a whole number that isn't negative.
bool ParseHops(std::string_view text, int& hops) {
  auto [end, failed] =
      std::from_chars(text.data(), text.data() + text.size(), hops);
  if (failed == std::errc() && end == text.data() + text.size() && hops >= 0) {
    return true;
  }
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
  std::string error;
  Clock::time_point start = Clock::now();
  if (!smollergraph::LoadFacts(file, graph, error)) {
    std::cerr << "sg: can't read the fact " << error << '\n';
    return 1;
  }
  std::chrono::duration<double> took = Clock::now() - start;
  std::cout << graph.node_count() << " nodes, " << graph.relationship_count()
            << " relationships, in " << std::fixed << std::setprecision(1)
            << took.count() << " s\n";
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
  for (smollergraph::NodeId start : starts) {
    Clock::time_point begin = Clock::now();
    smollergraph::Walk(graph, start, type, hops);
    std::chrono::duration<double, std::micro> took = Clock::now() - begin;
    micros.push_back(took.count());
  }
  std::sort(micros.begin(), micros.end());
  std::cout << std::fixed << std::setprecision(1) << "{\"open_ms\": " << open_ms
            << ", \"queries\": " << micros.size()
            << ", \"results\": " << results
            << ", \"median_us\": " << micros[micros.size() / 2]
            << ", \"p95_us\": " << micros[micros.size() * 95 / 100] << "}\n";
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  std::string_view command = argc > 1 ? argv[1] : "";
  bool load = command == "load" && argc == 4;
  bool walk = command == "walk" && argc == 6;
  bool bench = command == "bench" && argc == 6;
  if (!load && !walk && !bench) return Usage();

  smollergraph::Graph graph;
  std::string error;
  Clock::time_point opening = Clock::now();
  if (!graph.Open(argv[2], error)) {
    std::cerr << "sg: can't open the graph in " << argv[2] << ": " << error
              << '\n';
    return 1;
  }
  std::chrono::duration<double, std::milli> opened = Clock::now() - opening;
  if (load) return Load(graph, argv[3]);
  if (walk) return Walk(graph, argv[3], argv[4], argv[5]);
  return Bench(graph, argv[3], argv[4], argv[5], opened.count());
}
