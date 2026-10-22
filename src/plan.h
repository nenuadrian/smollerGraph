#ifndef SMOLLERGRAPH_PLAN_H_
#define SMOLLERGRAPH_PLAN_H_

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "parser.h"

namespace smollergraph {

// One step of a plan. A plan works on rows, which have a slot for each node
// of the pattern, and each step turns the rows it's given into the rows for
// the next step. The steps take their names from Neo4j's plans.
struct Step {
  enum class Kind {
    // A row for the node named `text`, in slot `to`, found in the index.
    kNodeIndexSeek,
    // A row for each node in the graph, in slot `to`.
    kAllNodesScan,
    // For each row, a row for each node that a relationship of type `text`
    // goes to from the node in slot `from`, in slot `to`.
    kExpand,
    // The rows whose node in slot `from` is named `text`.
    kFilter,
  };
  Kind kind;
  int from = -1;
  int to = -1;
  std::string text;
};

// How to answer a query: where to start, and where to go from there.
struct Plan {
  std::vector<Step> steps;
  // The pattern's variable for each slot, or nothing for a node it didn't
  // give one.
  std::vector<std::string> slots;
  // The slots whose names are returned, in order, and their columns' headings.
  std::vector<int> returns;
  std::vector<std::string> columns;
  bool distinct = false;
};

// Plans a query. Returns an error when the query returns a variable its
// pattern doesn't have, or gives two nodes one variable.
absl::StatusOr<Plan> PlanQuery(const Query& query);

// The plan's steps, a line each, in the order they run, as EXPLAIN shows them.
std::string Explain(const Plan& plan);

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_PLAN_H_
