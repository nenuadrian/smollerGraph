#include "plan.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <utility>
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

// A name on a node after the start keeps the rows that reach a node with that
// name, with a filter on the node's slot.
void AddFilter(const NodePattern& node, int slot, Plan& plan) {
  if (node.name) {
    plan.steps.push_back({Step::Kind::kFilter, slot, -1, *node.name});
  }
}

}  // namespace

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

  // A start with a name is found in the index. Without one, each node in the
  // graph is a start.
  const NodePattern& start = query.nodes[0];
  plan.slots.push_back(start.variable);
  if (start.name) {
    plan.steps.push_back({Step::Kind::kNodeIndexSeek, -1, 0, *start.name});
  } else {
    plan.steps.push_back({Step::Kind::kAllNodesScan, -1, 0, ""});
  }
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
    AddFilter(node, static_cast<int>(plan.slots.size()) - 1, plan);
  }

  plan.distinct = query.distinct;
  for (const std::string& variable : query.returns) {
    auto found = std::find(plan.slots.begin(), plan.slots.end(), variable);
    if (found == plan.slots.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("there's no variable called ", variable));
    }
    plan.returns.push_back(static_cast<int>(found - plan.slots.begin()));
    plan.columns.push_back(absl::StrCat(variable, ".name"));
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
        case Step::Kind::kExpand:
          for (NodeId node : graph.Neighbours(row[step.from], step.text)) {
            next.push_back(row);
            next.back()[step.to] = node;
          }
          break;
        case Step::Kind::kFilter:
          if (graph.name(row[step.from]) == step.text) next.push_back(row);
          break;
      }
    }
    rows = std::move(next);
  }

  Result result;
  result.columns = plan.columns;
  std::set<std::vector<NodeId>> seen;
  for (const std::vector<NodeId>& row : rows) {
    std::vector<NodeId> returned;
    returned.reserve(plan.returns.size());
    for (int slot : plan.returns) returned.push_back(row[slot]);
    // DISTINCT keeps the first of rows that return the same nodes.
    if (plan.distinct && !seen.insert(returned).second) continue;
    result.rows.push_back(std::move(returned));
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
      case Step::Kind::kExpand:
        absl::StrAppend(&text, "Expand (", SlotName(plan, step.from),
                        ")-[:", step.text, "]->(", SlotName(plan, step.to),
                        ")\n");
        break;
      case Step::Kind::kFilter:
        absl::StrAppend(&text, "Filter (", SlotName(plan, step.from),
                        " {name: '", step.text, "'})\n");
        break;
    }
  }
  absl::StrAppend(&text, plan.distinct ? "Return DISTINCT " : "Return ",
                  absl::StrJoin(plan.columns, ", "), "\n");
  return text;
}

}  // namespace smollergraph
