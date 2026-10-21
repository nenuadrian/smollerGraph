#include <charconv>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include "facts.h"
#include "graph.h"
#include "query.h"

int main(int argc, char** argv) {
  if (argc != 5) {
    std::cerr << "usage: sg <facts file> <start> <relationship type> <hops>\n";
    return 1;
  }
  std::ifstream file(argv[1]);
  if (!file) {
    std::cerr << "sg: can't open " << argv[1] << '\n';
    return 1;
  }
  smollergraph::Graph graph;
  std::string error;
  if (!smollergraph::LoadFacts(file, graph, error)) {
    std::cerr << "sg: can't read the fact " << error << '\n';
    return 1;
  }

  std::optional<smollergraph::NodeId> start = graph.Find(argv[2]);
  if (!start) {
    std::cerr << "sg: there's no node called " << argv[2] << '\n';
    return 1;
  }
  std::string_view text = argv[4];
  int hops = 0;
  auto [end, failed] =
      std::from_chars(text.data(), text.data() + text.size(), hops);
  if (failed != std::errc() || end != text.data() + text.size() || hops < 0) {
    std::cerr << "sg: hops must be a whole number, not " << text << '\n';
    return 1;
  }

  for (smollergraph::NodeId node :
       smollergraph::Walk(graph, *start, argv[3], hops)) {
    std::cout << graph.name(node) << '\n';
  }
  return 0;
}
