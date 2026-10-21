#include "graph.h"

namespace smollergraph {

NodeId Graph::AddNode(std::string_view name) {
  NodeId next = static_cast<NodeId>(nodes_.size());
  auto [it, added] = ids_.try_emplace(std::string(name), next);
  if (added) {
    nodes_.push_back({.name = std::string(name), .out = {}});
  }
  return it->second;
}

void Graph::AddRelationship(NodeId from, std::string_view type, NodeId to) {
  RelationshipId id = static_cast<RelationshipId>(relationships_.size());
  relationships_.push_back({.type = std::string(type), .from = from, .to = to});
  nodes_[from].out.push_back(id);
}

std::vector<NodeId> Graph::Neighbours(NodeId node,
                                      std::string_view type) const {
  std::vector<NodeId> found;
  for (RelationshipId id : nodes_[node].out) {
    if (relationships_[id].type == type) {
      found.push_back(relationships_[id].to);
    }
  }
  return found;
}

}  // namespace smollergraph
