#include "plan.h"

#include <algorithm>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"

namespace smollergraph {
namespace {

// A slot's variable, or _ and its number for a node the pattern didn't name.
std::string SlotName(const Plan& plan, int slot) {
  const std::string& variable = plan.slots[slot];
  return variable.empty() ? absl::StrCat("_", slot) : variable;
}

// A value as Cypher writes it: a string in quotes, and a number as it is.
std::string Literal(const Value& value) {
  if (const std::string* text = std::get_if<std::string>(&value)) {
    return absl::StrCat("'", *text, "'");
  }
  return ValueText(value);
}

// The name a node of the pattern has, if it gives it one, which the index can
// find.
std::optional<std::string> NameOf(const NodePattern& node) {
  for (const Property& property : node.properties) {
    const std::string* name = std::get_if<std::string>(&property.value);
    if (property.key == "name" && name != nullptr) return *name;
  }
  return std::nullopt;
}

// Adds the filters on a node of the pattern, once the step `found` has put it
// in a slot: one for its label, one for each of its properties, and one for
// each comparison in WHERE on its variable. They come as early as they can, so
// the rows they drop are expanded no further. A seek has found the name
// already, and a label scan the label.
void AddFilters(const Query& query, const NodePattern& node, int slot,
                Step::Kind found, Plan& plan) {
  if (!node.label.empty() && found != Step::Kind::kNodeByLabelScan) {
    plan.steps.push_back({Step::Kind::kHasLabel, slot, -1, node.label});
  }
  for (const Property& property : node.properties) {
    if (found == Step::Kind::kNodeIndexSeek && property.key == "name") {
      continue;
    }
    plan.steps.push_back(
        {Step::Kind::kFilter, slot, -1, property.key, "=", property.value});
  }
  if (node.variable.empty()) return;
  for (const Comparison& comparison : query.where) {
    if (comparison.property.variable != node.variable) continue;
    plan.steps.push_back({Step::Kind::kFilter, slot, -1,
                          comparison.property.key, comparison.op,
                          comparison.value});
  }
}

// How costly a node of the pattern is to find, when the plan starts from it:
// a name is found in the index, a label by a scan that keeps fewer rows, and
// otherwise each node in the graph is a start.
int CostToFind(const NodePattern& node) {
  if (NameOf(node)) return 0;
  if (!node.label.empty()) return 1;
  return 2;
}

// Adds a step that finds a node of the pattern, in a slot of its own: a seek
// in the index for a node with a name, a scan of a label for a node with a
// label, and otherwise a scan of all the nodes. Then come the filters on it.
// Returns its slot.
int AddFind(const Query& query, const NodePattern& node, Plan& plan) {
  plan.slots.push_back(node.variable);
  int slot = static_cast<int>(plan.slots.size()) - 1;
  if (std::optional<std::string> name = NameOf(node)) {
    plan.steps.push_back({Step::Kind::kNodeIndexSeek, -1, slot, *name});
  } else if (!node.label.empty()) {
    plan.steps.push_back({Step::Kind::kNodeByLabelScan, -1, slot, node.label});
  } else {
    plan.steps.push_back({Step::Kind::kAllNodesScan, -1, slot, ""});
  }
  AddFilters(query, node, slot, plan.steps.back().kind, plan);
  return slot;
}

// A relationship as Cypher writes it, with its arrow pointing the way it's
// followed, or with no arrow when it's followed both ways.
std::string Arrow(Direction direction, std::string_view type) {
  return absl::StrCat(direction == Direction::kIn ? "<-[:" : "-[:", type,
                      direction == Direction::kOut ? "]->" : "]-");
}

// A row: a node in each of the plan's slots, and the path the plan found, for
// a plan that finds one.
struct Row {
  std::vector<NodeId> nodes;
  Path path;
};

// Adds the expands from the node in `slot` to the pattern's `node`, along
// `relationship` in `direction`, then the filters on `node`, and returns the
// slot it ends up in.
int AddExpands(const Query& query, const RelationshipPattern& relationship,
               Direction direction, const NodePattern& node, int slot,
               Plan& plan) {
  // Each hop is an expand into a slot of its own. The last hop's slot is the
  // pattern's node, and the ones before it are nodes it doesn't name.
  for (int hop = 0; hop < relationship.hops; ++hop) {
    bool last = hop == relationship.hops - 1;
    plan.slots.push_back(last ? node.variable : "");
    int to = static_cast<int>(plan.slots.size()) - 1;
    plan.steps.push_back({.kind = Step::Kind::kExpand,
                          .from = slot,
                          .to = to,
                          .text = relationship.type,
                          .direction = direction});
    slot = to;
  }
  AddFilters(query, node, slot, Step::Kind::kExpand, plan);
  return slot;
}

// Whether a property compares with a value as `op` says. A missing property
// is Cypher's null, which no comparison keeps. Values of different types are
// never equal, and have no order.
bool Compare(const std::optional<Value>& property, const std::string& op,
             const Value& value) {
  if (!property) return false;
  if (property->index() != value.index()) return op == "<>";
  std::strong_ordering order = *property <=> value;
  if (op == "=") return order == 0;
  if (op == "<>") return order != 0;
  if (op == "<") return order < 0;
  if (op == "<=") return order <= 0;
  if (op == ">") return order > 0;
  return order >= 0;
}

// Adds a column to the plan for each item the query returns, and returns the
// plan, or an error for an item it can't return.
absl::StatusOr<Plan> AddReturns(const Query& query, Plan plan) {
  plan.distinct = query.distinct;
  for (const ReturnItem& item : query.returns) {
    if (!query.path.empty() && item.variable == query.path) {
      // A path has no properties, so it's returned whole, or as its length.
      if (!item.key.empty()) {
        return absl::InvalidArgumentError(
            absl::StrCat("the path ", item.variable, " has no properties"));
      }
      plan.returns.push_back(
          {.heading = item.length ? absl::StrCat("length(", item.variable, ")")
                                  : item.variable,
           .length = item.length});
      continue;
    }
    auto found = std::find(plan.slots.begin(), plan.slots.end(), item.variable);
    if (found == plan.slots.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("there's no variable called ", item.variable));
    }
    if (item.key.empty() || item.length) {
      return absl::InvalidArgumentError(
          absl::StrCat("a node is returned by its properties, such as ",
                       item.variable, ".name"));
    }
    plan.returns.push_back(
        {.heading = absl::StrCat(item.variable, ".", item.key),
         .slot = static_cast<int>(found - plan.slots.begin()),
         .key = item.key});
  }
  return plan;
}

}  // namespace

std::string ValueText(const Value& value) {
  if (const int64_t* number = std::get_if<int64_t>(&value)) {
    return absl::StrCat(*number);
  }
  return std::get<std::string>(value);
}

absl::StatusOr<Plan> PlanQuery(const Query& query) {
  Plan plan;
  for (size_t i = 0; i < query.nodes.size(); ++i) {
    const std::string& variable = query.nodes[i].variable;
    for (size_t j = 0; j < i; ++j) {
      if (!variable.empty() && query.nodes[j].variable == variable) {
        return absl::InvalidArgumentError(
            absl::StrCat("the variable ", variable, " is on two nodes"));
      }
    }
  }

  for (const Comparison& comparison : query.where) {
    const std::string& variable = comparison.property.variable;
    if (std::none_of(query.nodes.begin(), query.nodes.end(),
                     [&](const NodePattern& node) {
                       return node.variable == variable;
                     })) {
      return absl::InvalidArgumentError(
          absl::StrCat("there's no variable called ", variable));
    }
  }

  for (const RelationshipPattern& relationship : query.relationships) {
    if (relationship.hops == kAnyHops && query.path.empty()) {
      return absl::InvalidArgumentError(
          "a relationship with any number of hops needs shortestPath");
    }
  }

  if (!query.path.empty()) {
    if (query.relationships.size() != 1 ||
        query.relationships[0].hops != kAnyHops) {
      return absl::InvalidArgumentError(
          "shortestPath needs two nodes, with a relationship of any number of "
          "hops between them, such as (a)-[:KNOWS*]-(b)");
    }
    // Both ends are found, each in the way that's cheapest for it, and then
    // the path between them.
    int from = AddFind(query, query.nodes[0], plan);
    int to = AddFind(query, query.nodes[1], plan);
    plan.steps.push_back({.kind = Step::Kind::kShortestPath,
                          .from = from,
                          .to = to,
                          .text = query.relationships[0].type,
                          .direction = query.relationships[0].direction});
    return AddReturns(query, std::move(plan));
  }

  // The plan starts from the node of the pattern that's cheapest to find, the
  // first of them when there's a tie.
  size_t first = 0;
  for (size_t i = 1; i < query.nodes.size(); ++i) {
    if (CostToFind(query.nodes[i]) < CostToFind(query.nodes[first])) first = i;
  }
  AddFind(query, query.nodes[first], plan);
  // From the start, it expands to the pattern's right end, then to its left
  // end, following each relationship there from the node after it, so the
  // other way.
  int slot = 0;
  for (size_t i = first; i < query.relationships.size(); ++i) {
    slot = AddExpands(query, query.relationships[i],
                      query.relationships[i].direction, query.nodes[i + 1],
                      slot, plan);
  }
  slot = 0;
  for (size_t i = first; i > 0; --i) {
    const RelationshipPattern& relationship = query.relationships[i - 1];
    slot = AddExpands(query, relationship, Reversed(relationship.direction),
                      query.nodes[i - 1], slot, plan);
  }
  return AddReturns(query, std::move(plan));
}

Result Run(const Plan& plan, const Graph& graph) {
  // A single row to begin with, with its slots still to fill.
  std::vector<Row> rows = {{std::vector<NodeId>(plan.slots.size()), {}}};
  for (const Step& step : plan.steps) {
    std::vector<Row> next;
    for (const Row& row : rows) {
      switch (step.kind) {
        case Step::Kind::kNodeIndexSeek:
          if (std::optional<NodeId> node = graph.Find(step.text)) {
            next.push_back(row);
            next.back().nodes[step.to] = *node;
          }
          break;
        case Step::Kind::kAllNodesScan:
          for (NodeId node = 0; node < graph.node_count(); ++node) {
            next.push_back(row);
            next.back().nodes[step.to] = node;
          }
          break;
        case Step::Kind::kNodeByLabelScan:
          for (NodeId node = 0; node < graph.node_count(); ++node) {
            if (graph.label(node) != step.text) continue;
            next.push_back(row);
            next.back().nodes[step.to] = node;
          }
          break;
        case Step::Kind::kExpand:
          for (NodeId node : graph.Neighbours(row.nodes[step.from], step.text,
                                              step.direction)) {
            next.push_back(row);
            next.back().nodes[step.to] = node;
          }
          break;
        case Step::Kind::kFilter:
          if (Compare(graph.FindProperty(row.nodes[step.from], step.text),
                      step.op, step.value)) {
            next.push_back(row);
          }
          break;
        case Step::Kind::kHasLabel:
          if (graph.label(row.nodes[step.from]) == step.text) {
            next.push_back(row);
          }
          break;
        case Step::Kind::kShortestPath: {
          PathSearch search =
              ShortestPath(graph, row.nodes[step.from], row.nodes[step.to],
                           step.text, step.direction);
          if (search.path) {
            next.push_back(row);
            next.back().path = *std::move(search.path);
          }
          break;
        }
      }
    }
    rows = std::move(next);
  }

  Result result;
  for (const Column& column : plan.returns) {
    result.columns.push_back(column.heading);
  }
  std::set<std::vector<std::optional<Value>>> seen;
  for (const Row& row : rows) {
    std::vector<Cell> cells;
    std::vector<std::optional<Value>> values;
    for (const Column& column : plan.returns) {
      if (column.slot >= 0) {
        NodeId node = row.nodes[column.slot];
        cells.push_back(
            {.node = node, .value = graph.FindProperty(node, column.key)});
      } else if (column.length) {
        int64_t length = row.path.relationships.size();
        cells.push_back({row.path.nodes[0], length, row.path});
      } else {
        cells.push_back(
            {row.path.nodes[0], PathText(graph, row.path), row.path});
      }
      values.push_back(cells.back().value);
    }
    // DISTINCT keeps the first of rows that return the same values.
    if (plan.distinct && !seen.insert(std::move(values)).second) continue;
    result.rows.push_back(std::move(cells));
  }
  return result;
}

std::string Explain(const Plan& plan) {
  std::string text;
  for (const Step& step : plan.steps) {
    switch (step.kind) {
      case Step::Kind::kNodeIndexSeek:
        absl::StrAppend(&text, "NodeIndexSeek (", SlotName(plan, step.to),
                        " {name: '", step.text, "'})\n");
        break;
      case Step::Kind::kAllNodesScan:
        absl::StrAppend(&text, "AllNodesScan (", SlotName(plan, step.to),
                        ")\n");
        break;
      case Step::Kind::kNodeByLabelScan:
        absl::StrAppend(&text, "NodeByLabelScan (", SlotName(plan, step.to),
                        ":", step.text, ")\n");
        break;
      case Step::Kind::kExpand:
        absl::StrAppend(&text, "Expand (", SlotName(plan, step.from), ")",
                        Arrow(step.direction, step.text), "(",
                        SlotName(plan, step.to), ")\n");
        break;
      case Step::Kind::kFilter:
        absl::StrAppend(&text, "Filter ", SlotName(plan, step.from), ".",
                        step.text, " ", step.op, " ", Literal(step.value),
                        "\n");
        break;
      case Step::Kind::kHasLabel:
        absl::StrAppend(&text, "Filter ", SlotName(plan, step.from), ":",
                        step.text, "\n");
        break;
      case Step::Kind::kShortestPath:
        // A star after the type, as the path has any number of hops.
        absl::StrAppend(&text, "ShortestPath (", SlotName(plan, step.from), ")",
                        Arrow(step.direction, step.text + "*"), "(",
                        SlotName(plan, step.to), ")\n");
        break;
    }
  }
  std::vector<std::string> headings;
  headings.reserve(plan.returns.size());
  for (const Column& column : plan.returns) headings.push_back(column.heading);
  absl::StrAppend(&text, plan.distinct ? "Return DISTINCT " : "Return ",
                  absl::StrJoin(headings, ", "), "\n");
  return text;
}

}  // namespace smollergraph
