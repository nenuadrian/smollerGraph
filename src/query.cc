#include "query.h"

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
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

std::string PathText(const Graph& graph, const Path& path) {
  std::string text = absl::StrCat("(", graph.name(path.nodes[0]), ")");
  for (size_t i = 0; i < path.relationships.size(); ++i) {
    RelationshipId id = path.relationships[i];
    bool out = graph.relationship(id).from == path.nodes[i];
    absl::StrAppend(&text, out ? "-[:" : "<-[:", graph.type(id),
                    out ? "]->(" : "]-(", graph.name(path.nodes[i + 1]), ")");
  }
  return text;
}

}  // namespace smollergraph
