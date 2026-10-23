#ifndef SMOLLERGRAPH_PLAN_H_
#define SMOLLERGRAPH_PLAN_H_

#include <optional>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "graph.h"
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
    // A row for each node with the label `text`, in slot `to`. With no index
    // on labels, it reads each node's record to find them.
    kNodeByLabelScan,
    // For each row, a row for each node that a relationship of type `text`
    // goes to from the node in slot `from`, in slot `to`.
    kExpand,
    // The rows whose node in slot `from` has a property `text` that compares
    // with `value` as `op` says.
    kFilter,
    // The rows whose node in slot `from` has the label `text`.
    kHasLabel,
  };
  Kind kind;
  int from = -1;
  int to = -1;
  std::string text;
  // A filter's operator, and the value it compares with.
  std::string op = "=";
  Value value{};
};

// How to answer a query: where to start, and where to go from there.
struct Plan {
  std::vector<Step> steps;
  // The pattern's variable for each slot, or nothing for a node it didn't
  // give one.
  std::vector<std::string> slots;
  // The slots whose properties are returned, in order, the properties' keys,
  // and their columns' headings.
  std::vector<int> returns;
  std::vector<std::string> keys;
  std::vector<std::string> columns;
  bool distinct = false;
};

// Plans a query. Returns an error when the query compares or returns a
// variable its pattern doesn't have, or gives two nodes one variable.
absl::StatusOr<Plan> PlanQuery(const Query& query);

// The plan's steps, a line each, in the order they run, as EXPLAIN shows them.
std::string Explain(const Plan& plan);

// A value a query returns, and the node it's a property of. A node without
// the property returns Cypher's null, which is no value.
struct Cell {
  NodeId node;
  std::optional<Value> value;
};

// What a query returns: a row for each match of its pattern, with a cell for
// each column.
struct Result {
  std::vector<std::string> columns;
  std::vector<std::vector<Cell>> rows;
};

// A value as sg query prints it: a number in digits, and a string as it is.
std::string ValueText(const Value& value);

// Runs a plan on a graph, a step at a time, each on all of the rows at once.
Result Run(const Plan& plan, const Graph& graph);

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_PLAN_H_
