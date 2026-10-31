#include "query.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
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

// The nodes a search for a cheapest path has still to settle, each with the
// cost it was put in with, cheapest first. A priority queue keeps its greatest
// at the top, and std::greater turns that round, so the top is the least.
using Entry = std::pair<int64_t, NodeId>;
using Queue = std::priority_queue<Entry, std::vector<Entry>, std::greater<>>;

// One end of a search for a cheapest path from both ends: the direction it
// follows relationships in, the cheapest cost it's found to each node it has
// reached, which can go down until the node is settled, with the hop that gave
// it, and the nodes it has still to settle.
struct WeightedSide {
  Direction direction;
  std::unordered_map<NodeId, int64_t> costs;
  Parents parents;
  Queue queue;
};

// The cost of the next node `side` settles, or nothing once it has settled
// all it can reach. A node goes in the queue again each time its cost goes
// down, so the entries with its old costs are dropped from the top first.
std::optional<int64_t> NextCost(WeightedSide& side) {
  while (!side.queue.empty() &&
         side.queue.top().first > side.costs.at(side.queue.top().second)) {
    side.queue.pop();
  }
  if (side.queue.empty()) return std::nullopt;
  return side.queue.top().first;
}

// Settles the node at the top of `side`'s queue, and lowers the costs of its
// neighbours through it. A neighbour that the other side has reached is a way
// from one end to the other, and `best` keeps the cheapest of those, with the
// node where its halves meet.
void Settle(const Graph& graph, std::string_view type, std::string_view key,
            WeightedSide& side, const WeightedSide& other, int64_t& best,
            NodeId& meet) {
  auto [cost, node] = side.queue.top();
  side.queue.pop();
  for (Hop hop : graph.Hops(node, type, side.direction)) {
    std::optional<int64_t> weight = Weight(graph, hop.relationship, key);
    if (!weight) continue;
    int64_t through = cost + *weight;
    auto [known, added] = side.costs.try_emplace(hop.node, through);
    if (!added && through >= known->second) continue;
    known->second = through;
    side.parents[hop.node] = {hop.relationship, node};
    side.queue.push({through, hop.node});
    auto there = other.costs.find(hop.node);
    if (there != other.costs.end() && through + there->second < best) {
      best = through + there->second;
      meet = hop.node;
    }
  }
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
  if (from == to) return {Path{{from}, {}}, 1};
  WeightedSide start = {.direction = direction,
                        .costs = {{from, 0}},
                        .parents = {{from, {kNoRelationship, from}}}};
  WeightedSide end = {.direction = Reversed(direction),
                      .costs = {{to, 0}},
                      .parents = {{to, {kNoRelationship, to}}}};
  start.queue.push({0, from});
  end.queue.push({0, to});
  int64_t best = std::numeric_limits<int64_t>::max();
  NodeId meet = from;
  // Each round settles a node of the side whose next one is cheaper. A path
  // through nodes neither side has settled costs at least the two next costs
  // added up, so once they add up to the best way found, there's no cheaper.
  while (true) {
    std::optional<int64_t> forwards = NextCost(start);
    std::optional<int64_t> backwards = NextCost(end);
    if (!forwards || !backwards || *forwards + *backwards >= best) break;
    if (*forwards <= *backwards) {
      Settle(graph, type, key, start, end, best, meet);
    } else {
      Settle(graph, type, key, end, start, best, meet);
    }
  }
  size_t reached = start.costs.size() + end.costs.size();
  if (best == std::numeric_limits<int64_t>::max())
    return {std::nullopt, reached};
  Path path = PathTo(start.parents, meet);
  Path back = PathTo(end.parents, meet);
  path.nodes.insert(path.nodes.end(), back.nodes.rbegin() + 1,
                    back.nodes.rend());
  path.relationships.insert(path.relationships.end(),
                            back.relationships.rbegin(),
                            back.relationships.rend());
  path.cost = best;
  return {path, reached};
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
