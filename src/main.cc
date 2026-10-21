#include <charconv>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include "facts.h"
#include "graph.h"
#include "query.h"

namespace {

int Usage() {
  std::cerr
      << "usage: sg load <graph folder> <facts file>\n"
         "       sg walk <graph folder> <start> <relationship type> <hops>\n";
  return 1;
}

// Adds the facts in the file at `path` to the graph.
int Load(smollergraph::Graph& graph, const char* path) {
  std::ifstream file(path);
  if (!file) {
    std::cerr << "sg: can't open " << path << '\n';
    return 1;
  }
  std::string error;
  if (!smollergraph::LoadFacts(file, graph, error)) {
    std::cerr << "sg: can't read the fact " << error << '\n';
    return 1;
  }
  std::cout << graph.node_count() << " nodes, " << graph.relationship_count()
            << " relationships\n";
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
  auto [end, failed] =
      std::from_chars(text.data(), text.data() + text.size(), hops);
  if (failed != std::errc() || end != text.data() + text.size() || hops < 0) {
    std::cerr << "sg: hops must be a whole number, not " << text << '\n';
    return 1;
  }
  for (smollergraph::NodeId found :
       smollergraph::Walk(graph, *node, type, hops)) {
    std::cout << graph.name(found) << '\n';
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  std::string_view command = argc > 1 ? argv[1] : "";
  bool load = command == "load" && argc == 4;
  bool walk = command == "walk" && argc == 6;
  if (!load && !walk) return Usage();

  smollergraph::Graph graph;
  std::string error;
  if (!graph.Open(argv[2], error)) {
    std::cerr << "sg: can't open the graph in " << argv[2] << ": " << error
              << '\n';
    return 1;
  }
  return load ? Load(graph, argv[3]) : Walk(graph, argv[3], argv[4], argv[5]);
}
