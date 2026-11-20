// Builds a small graph, asks it a question in Cypher, and checks the answer,
// with nothing but smollerGraph's library.
#include <iostream>
#include <sstream>

#include "absl/status/statusor.h"
#include "facts.h"
#include "graph.h"
#include "parser.h"
#include "plan.h"

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
  absl::StatusOr<smollergraph::Query> query = smollergraph::Parse(
      "MATCH (a {name: 'Red fox'})-[:EATS]->(b)-[:LIVES_IN]->(c) "
      "RETURN c.name");
  absl::StatusOr<smollergraph::Plan> plan =
      query.ok() ? smollergraph::PlanQuery(*query) : query.status();
  if (!plan.ok()) {
    std::cerr << plan.status() << '\n';
    return 1;
  }
  smollergraph::Result result = smollergraph::Run(*plan, graph);
  // The fox eats the rabbit, which lives in the meadow.
  if (result.rows.size() != 1 ||
      smollergraph::ValueText(*result.rows[0][0].value) != "Meadow") {
    std::cerr << "the fox's food doesn't live in the meadow\n";
    return 1;
  }
  std::cout << "The red fox's food lives in the Meadow\n";
  return 0;
}
