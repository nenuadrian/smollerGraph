#include "query.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "absl/strings/str_cat.h"

namespace smollergraph {
namespace {

// Each node a search has reached, with the hop that reached it: the
// relationship, and the node the search came from.
using Parents = std::unordered_map<NodeId, Hop>;

// The path from the search's start to `node`, read backwards from the node,
// through each node's parent, to the start, whose parent is no relationship.
Path PathTo(const Parents& parents, NodeId node) {
  Path path = {{node}, {}};
  for (Hop parent = parents.at(node); parent.relationship != kNoRelationship;
       parent = parents.at(parent.node)) {
    path.relationships.push_back(parent.relationship);
    path.nodes.push_back(parent.node);
  }
  std::reverse(path.nodes.begin(), path.nodes.end());
  std::reverse(path.relationships.begin(), path.relationships.end());
  return path;
}

// One end of a search from both ends: the direction it follows relationships
// in, each node it has reached, with its parent, and the nodes it reached last,
// which it goes on from.
struct Side {
  Direction direction;
  Parents parents;
  std::vector<NodeId> frontier;
};

// Takes `side` a hop further, and returns the first node it reaches that the
// other side has reached too, if there's one, where the two searches meet.
std::optional<NodeId> Expand(const Graph& graph, std::string_view type,
                             Side& side, const Side& other) {
  std::vector<NodeId> next;
  for (NodeId node : side.frontier) {
    for (Hop hop : graph.Hops(node, type, side.direction)) {
      Hop parent = {hop.relationship, node};
      if (!side.parents.try_emplace(hop.node, parent).second) continue;
      if (other.parents.contains(hop.node)) return hop.node;
      next.push_back(hop.node);
    }
  }
  side.frontier = std::move(next);
  return std::nullopt;
}

// A relationship's weight, its property `key`, when it's a whole number of
// zero or more, as Dijkstra's algorithm needs.
std::optional<int64_t> Weight(const Graph& graph, RelationshipId relationship,
                              std::string_view key) {
  std::optional<Value> value =
      graph.FindRelationshipProperty(relationship, key);
  const int64_t* weight = value ? std::get_if<int64_t>(&*value) : nullptr;
  if (weight == nullptr || *weight < 0) return std::nullopt;
  return *weight;
}

// A property's value as Cypher writes it: a string in quotes, and a number as
// it is.
std::string Literal(const Value& value) {
  if (const std::string* text = std::get_if<std::string>(&value)) {
    return absl::StrCat("'", *text, "'");
  }
  return absl::StrCat(std::get<int64_t>(value));
}

}  // namespace

std::vector<NodeId> Walk(const Graph& graph, NodeId start,
                         std::string_view type, int hops) {
  std::vector<std::vector<NodeId>> reached = WalkHops(graph, start, type, hops);
  return std::move(reached.back());
}

std::vector<std::vector<NodeId>> WalkHops(const Graph& graph, NodeId start,
                                          std::string_view type, int hops) {
  std::vector<std::vector<NodeId>> reached = {{start}};
  for (int hop = 0; hop < hops; ++hop) {
    std::vector<NodeId> next;
    for (NodeId node : reached.back()) {
      for (NodeId neighbour : graph.Neighbours(node, type)) {
        // Two paths can lead to one node, and it's listed once.
        if (std::find(next.begin(), next.end(), neighbour) == next.end()) {
          next.push_back(neighbour);
        }
      }
    }
    reached.push_back(std::move(next));
  }
  return reached;
}

PathSearch ShortestPath(const Graph& graph, NodeId from, NodeId to,
                        std::string_view type, Direction direction) {
  if (from == to) return {Path{{from}, {}}, 1};
  // Two breadth-first searches, one from each end, each a hop at a time, until
  // they meet. The one from `to` follows the relationships the other way, as
  // it walks the path backwards.
  Side start = {direction, {{from, {kNoRelationship, from}}}, {from}};
  Side end = {Reversed(direction), {{to, {kNoRelationship, to}}}, {to}};
  while (!start.frontier.empty() && !end.frontier.empty()) {
    // The side with fewer nodes to go on from takes the next hop, as it's
    // likely to read fewer relationships.
    bool forwards = start.frontier.size() <= end.frontier.size();
    std::optional<NodeId> met = forwards ? Expand(graph, type, start, end)
                                         : Expand(graph, type, end, start);
    if (!met) continue;
    // The start's half of the path runs to where they met, and the end's half
    // runs from the end to there, so it's added backwards.
    Path path = PathTo(start.parents, *met);
    Path back = PathTo(end.parents, *met);
    path.nodes.insert(path.nodes.end(), back.nodes.rbegin() + 1,
                      back.nodes.rend());
    path.relationships.insert(path.relationships.end(),
                              back.relationships.rbegin(),
                              back.relationships.rend());
    return {path, start.parents.size() + end.parents.size() - 1};
  }
  return {std::nullopt, start.parents.size() + end.parents.size()};
}

PathSearch CheapestPath(const Graph& graph, NodeId from, NodeId to,
                        std::string_view type, Direction direction,
                        std::string_view key) {
  // The cheapest cost found so far to each node reached, which can go down
  // until the node is settled, and the hop that gave it.
  std::unordered_map<NodeId, int64_t> costs = {{from, 0}};
  Parents parents = {{from, {kNoRelationship, from}}};
  // The nodes to settle, cheapest first. A priority queue keeps its greatest
  // at the top, and std::greater turns that round, so the top is the least.
  using Entry = std::pair<int64_t, NodeId>;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> queue;
  queue.push({0, from});
  while (!queue.empty()) {
    auto [cost, node] = queue.top();
    queue.pop();
    // A node goes in the queue again each time its cost goes down, and the
    // entries with the old costs are skipped.
    if (cost > costs.at(node)) continue;
    // The cheapest of the nodes left can't get any cheaper, as the weights
    // are zero or more, so the end is settled once it comes out.
    if (node == to) {
      Path path = PathTo(parents, to);
      path.cost = cost;
      return {path, costs.size()};
    }
    for (Hop hop : graph.Hops(node, type, direction)) {
      std::optional<int64_t> weight = Weight(graph, hop.relationship, key);
      if (!weight) continue;
      int64_t through = cost + *weight;
      auto [known, added] = costs.try_emplace(hop.node, through);
      if (!added && through >= known->second) continue;
      known->second = through;
      parents[hop.node] = {hop.relationship, node};
      queue.push({through, hop.node});
    }
  }
  return {std::nullopt, costs.size()};
}

std::string PathText(const Graph& graph, const Path& path) {
  std::string text = absl::StrCat("(", graph.name(path.nodes[0]), ")");
  for (size_t i = 0; i < path.relationships.size(); ++i) {
    RelationshipId id = path.relationships[i];
    bool out = graph.relationship(id).from == path.nodes[i];
    std::string properties;
    for (const Property& property : graph.RelationshipProperties(id)) {
      absl::StrAppend(&properties, properties.empty() ? " {" : ", ",
                      property.key, ": ", Literal(property.value));
    }
    if (!properties.empty()) properties += "}";
    absl::StrAppend(&text, out ? "-[:" : "<-[:", graph.type(id), properties,
                    out ? "]->(" : "]-(", graph.name(path.nodes[i + 1]), ")");
  }
  return text;
}

}  // namespace smollergraph
