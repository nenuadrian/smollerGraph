#include "query.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace smollergraph {

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

}  // namespace smollergraph
