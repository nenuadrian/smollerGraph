#ifndef SMOLLERGRAPH_QUERY_H_
#define SMOLLERGRAPH_QUERY_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "graph.h"

namespace smollergraph {

// Returns the nodes exactly `hops` relationships of `type` away from `start`,
// each once, in the order a breadth-first walk reaches them.
std::vector<NodeId> Walk(const Graph& graph, NodeId start,
                         std::string_view type, int hops);

// Returns the nodes each hop of that walk reaches: the start on its own, then
// the nodes one hop away, and so on, so the last of them is what Walk returns.
std::vector<std::vector<NodeId>> WalkHops(const Graph& graph, NodeId start,
                                          std::string_view type, int hops);

// A path through the graph: its nodes, from its start to its end, and the
// relationships between them, one fewer.
struct Path {
  std::vector<NodeId> nodes;
  std::vector<RelationshipId> relationships;
};

// What a search for a path found: the path, if there is one, and how many
// nodes the search reached, which is most of what it cost.
struct PathSearch {
  std::optional<Path> path;
  size_t reached = 0;
};

// Returns a shortest path from `from` to `to`, along relationships of `type`
// followed in `direction`: one with as few hops as any other.
PathSearch ShortestPath(const Graph& graph, NodeId from, NodeId to,
                        std::string_view type, Direction direction);

// A path as Cypher writes it, with the nodes' names, and each relationship's
// arrow pointing the way it goes.
std::string PathText(const Graph& graph, const Path& path);

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_QUERY_H_
