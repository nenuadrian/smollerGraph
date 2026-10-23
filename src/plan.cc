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

  // A start with a name is found in the index. Without one, a start with a
  // label is found by its label, and without either, each node in the graph
  // is a start.
  const NodePattern& start = query.nodes[0];
  plan.slots.push_back(start.variable);
  std::optional<std::string> name = NameOf(start);
  if (name) {
    plan.steps.push_back({Step::Kind::kNodeIndexSeek, -1, 0, *name});
  } else if (!start.label.empty()) {
    plan.steps.push_back({Step::Kind::kNodeByLabelScan, -1, 0, start.label});
  } else {
    plan.steps.push_back({Step::Kind::kAllNodesScan, -1, 0, ""});
  }
  AddFilters(query, start, 0, plan.steps.back().kind, plan);
  for (size_t i = 0; i < query.relationships.size(); ++i) {
    const RelationshipPattern& relationship = query.relationships[i];
    const NodePattern& node = query.nodes[i + 1];
    // Each hop is an expand into a slot of its own. The last hop's slot is the
    // pattern's next node, and the ones before it are nodes it doesn't name.
    for (int hop = 0; hop < relationship.hops; ++hop) {
      int from = static_cast<int>(plan.slots.size()) - 1;
      bool last = hop == relationship.hops - 1;
      plan.slots.push_back(last ? node.variable : "");
      plan.steps.push_back(
          {Step::Kind::kExpand, from, from + 1, relationship.type});
    }
    AddFilters(query, node, static_cast<int>(plan.slots.size()) - 1,
               Step::Kind::kExpand, plan);
  }

  plan.distinct = query.distinct;
  for (const PropertyLookup& lookup : query.returns) {
    auto found =
        std::find(plan.slots.begin(), plan.slots.end(), lookup.variable);
    if (found == plan.slots.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("there's no variable called ", lookup.variable));
    }
    plan.returns.push_back(static_cast<int>(found - plan.slots.begin()));
    plan.keys.push_back(lookup.key);
    plan.columns.push_back(absl::StrCat(lookup.variable, ".", lookup.key));
  }
  return plan;
}

Result Run(const Plan& plan, const Graph& graph) {
  // A single row to begin with, with its slots still to fill.
  std::vector<std::vector<NodeId>> rows = {
      std::vector<NodeId>(plan.slots.size())};
  for (const Step& step : plan.steps) {
    std::vector<std::vector<NodeId>> next;
    for (const std::vector<NodeId>& row : rows) {
      switch (step.kind) {
        case Step::Kind::kNodeIndexSeek:
          if (std::optional<NodeId> node = graph.Find(step.text)) {
            next.push_back(row);
            next.back()[step.to] = *node;
          }
          break;
        case Step::Kind::kAllNodesScan:
          for (NodeId node = 0; node < graph.node_count(); ++node) {
            next.push_back(row);
            next.back()[step.to] = node;
          }
          break;
        case Step::Kind::kNodeByLabelScan:
          for (NodeId node = 0; node < graph.node_count(); ++node) {
            if (graph.label(node) != step.text) continue;
            next.push_back(row);
            next.back()[step.to] = node;
          }
          break;
        case Step::Kind::kExpand:
          for (NodeId node : graph.Neighbours(row[step.from], step.text)) {
            next.push_back(row);
            next.back()[step.to] = node;
          }
          break;
        case Step::Kind::kFilter:
          if (Compare(graph.FindProperty(row[step.from], step.text), step.op,
                      step.value)) {
            next.push_back(row);
          }
          break;
        case Step::Kind::kHasLabel:
          if (graph.label(row[step.from]) == step.text) next.push_back(row);
          break;
      }
    }
    rows = std::move(next);
  }

  Result result;
  result.columns = plan.columns;
  std::set<std::vector<std::optional<Value>>> seen;
  for (const std::vector<NodeId>& row : rows) {
    std::vector<Cell> cells;
    std::vector<std::optional<Value>> values;
    for (size_t i = 0; i < plan.returns.size(); ++i) {
      NodeId node = row[plan.returns[i]];
      cells.push_back({node, graph.FindProperty(node, plan.keys[i])});
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
        absl::StrAppend(&text, "Expand (", SlotName(plan, step.from),
                        ")-[:", step.text, "]->(", SlotName(plan, step.to),
                        ")\n");
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
    }
  }
  absl::StrAppend(&text, plan.distinct ? "Return DISTINCT " : "Return ",
                  absl::StrJoin(plan.columns, ", "), "\n");
  return text;
}

}  // namespace smollergraph
