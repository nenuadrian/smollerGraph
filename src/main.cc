#include <fstream>
#include <iostream>
#include <string>

#include "facts.h"
#include "graph.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: sg <facts file>\n";
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
  std::cout << graph.node_count() << " nodes, " << graph.relationship_count()
            << " relationships\n";
  return 0;
}
