#ifndef SMOLLERGRAPH_PLAN_H_
#define SMOLLERGRAPH_PLAN_H_

#include <optional>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "graph.h"
#include "parser.h"
#include "query.h"

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
    // goes to from the node in slot `from`, in slot `to`, or comes from, when
    // its direction is in.
    kExpand,
    // The rows whose node in slot `from` has a property `text` that compares
    // with `value` as `op` says.
    kFilter,
    // The rows whose node in slot `from` has the label `text`.
    kHasLabel,
    // For each row, a shortest path from the node in slot `from` to the node
    // in slot `to`, along relationships of type `text`, and no row when
    // there's no path.
    kShortestPath,
    // The same for a cheapest path, whose relationships' `weight` adds up to
    // the least.
    kCheapestPath,
  };
  Kind kind;
  int from = -1;
  int to = -1;
  std::string text;
  // A filter's operator, and the value it compares with.
  std::string op = "=";
  Value value{};
  // Which of its node's relationships an expand or a path follows.
  Direction direction = Direction::kOut;
  // The property a cheapest path adds up.
  std::string weight{};
};

// A column of what a plan returns: its heading, such as b.name, p or
// length(p), and the property of the node in a slot that it returns, or with
// no slot, the row's path, or a function of it, its length or its cost.
struct Column {
  std::string heading;
  int slot = -1;
  std::string key;
  std::string function;
};

// How to answer a query: where to start, and where to go from there.
struct Plan {
  std::vector<Step> steps;
  // The pattern's variable for each slot, or nothing for a node it didn't
  // give one.
  std::vector<std::string> slots;
  // What it returns, a column at a time, in order.
  std::vector<Column> returns;
  bool distinct = false;
};

// Plans a query, starting from the node of its pattern that's cheapest to
// find, or for a shortest path, finding both its nodes, and then the path.
// Returns an error when the query compares or returns a variable its pattern
// doesn't have, gives two nodes one variable, or has a pattern it can't plan.
absl::StatusOr<Plan> PlanQuery(const Query& query);

// The plan's steps, a line each, in the order they run, as EXPLAIN shows them.
std::string Explain(const Plan& plan);

// A value a query returns, and the node it's a property of. A node without
// the property returns Cypher's null, which is no value. A path, or a function
// of it, comes with the path, and its first node.
struct Cell {
  NodeId node;
  std::optional<Value> value;
  Path path;
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
