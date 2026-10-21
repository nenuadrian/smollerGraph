#include <iostream>

#include "graph.h"

int main() {
  smollergraph::Graph graph;
  smollergraph::NodeId fox = graph.AddNode("Red fox");
  graph.AddRelationship(fox, "EATS", graph.AddNode("Rabbit"));
  graph.AddRelationship(fox, "EATS", graph.AddNode("Field mouse"));
  graph.AddRelationship(fox, "LIVES_IN", graph.AddNode("Forest"));

  for (smollergraph::NodeId prey : graph.Neighbours(fox, "EATS")) {
    std::cout << graph.name(fox) << " eats " << graph.name(prey) << '\n';
  }
  return 0;
}
