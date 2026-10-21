#include "query.h"

#include <algorithm>
#include <utility>

namespace smollergraph {

std::vector<NodeId> Walk(const Graph& graph, NodeId start,
                         std::string_view type, int hops) {
  std::vector<NodeId> frontier = {start};
  for (int hop = 0; hop < hops; ++hop) {
    std::vector<NodeId> next;
    for (NodeId node : frontier) {
      for (NodeId neighbour : graph.Neighbours(node, type)) {
        // Two paths can lead to one node, and it's listed once.
        if (std::find(next.begin(), next.end(), neighbour) == next.end()) {
          next.push_back(neighbour);
        }
      }
    }
    frontier = std::move(next);
  }
  return frontier;
}

}  // namespace smollergraph
