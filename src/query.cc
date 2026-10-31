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
  Parents parents = {{from, {kNoRelationship, from}}};
  // A breadth-first search: the nodes one hop away, then the nodes one hop
  // further, and so on, so the first time it reaches `to`, it's by a path with
  // as few hops as any.
  std::vector<NodeId> frontier = {from};
  while (!frontier.empty()) {
    std::vector<NodeId> next;
    for (NodeId node : frontier) {
      for (Hop hop : graph.Hops(node, type, direction)) {
        Hop parent = {hop.relationship, node};
        // A node reached already was reached by a path as short, or shorter.
        if (!parents.try_emplace(hop.node, parent).second) continue;
        if (hop.node == to) return {PathTo(parents, to), parents.size()};
        next.push_back(hop.node);
      }
    }
    frontier = std::move(next);
  }
  return {std::nullopt, parents.size()};
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
