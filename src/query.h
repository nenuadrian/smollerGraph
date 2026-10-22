#ifndef SMOLLERGRAPH_QUERY_H_
#define SMOLLERGRAPH_QUERY_H_

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

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_QUERY_H_
