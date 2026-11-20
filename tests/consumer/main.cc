// Builds a small graph, asks it a question in Cypher, and checks the answer,
// with nothing but smollerGraph's library.
#include <iostream>
#include <sstream>

#include "absl/status/statusor.h"
#include "smollergraph/facts.h"
#include "smollergraph/graph.h"
#include "smollergraph/plan.h"

int main() {
  smollergraph::Graph graph;
  std::istringstream facts(
      "(Red fox)-[:EATS]->(Rabbit)\n"
      "(Rabbit)-[:LIVES_IN]->(Meadow)\n");
  if (absl::Status status = smollergraph::LoadFacts(facts, graph);
      !status.ok()) {
    std::cerr << status << '\n';
    return 1;
  }
  absl::StatusOr<smollergraph::Result> result = smollergraph::Execute(
      graph,
      "MATCH (a {name: 'Red fox'})-[:EATS]->(b)-[:LIVES_IN]->(c) "
      "RETURN c.name");
  if (!result.ok()) {
    std::cerr << result.status() << '\n';
    return 1;
  }
  // The fox eats the rabbit, which lives in the meadow.
  if (result->rows.size() != 1 ||
      smollergraph::ValueText(*result->rows[0][0].value) != "Meadow") {
    std::cerr << "the fox's food doesn't live in the meadow\n";
    return 1;
  }
  std::cout << "The red fox's food lives in the Meadow\n";
  return 0;
}
