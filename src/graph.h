#ifndef SMOLLERGRAPH_GRAPH_H_
#define SMOLLERGRAPH_GRAPH_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace smollergraph {

using NodeId = uint32_t;
using RelationshipId = uint32_t;

struct Node {
  std::string name;
  // The relationships that start at this node, so a hop needs no search.
  std::vector<RelationshipId> out;
};

struct Relationship {
  std::string type;
  NodeId from;
  NodeId to;
};

// A graph kept in memory: nodes by name, and relationships with a type and a
// direction.
class Graph {
 public:
  // Returns the node with this name, adding it the first time.
  NodeId AddNode(std::string_view name);
  void AddRelationship(NodeId from, std::string_view type, NodeId to);

  // Returns the nodes one hop from `node` along relationships of `type`.
  std::vector<NodeId> Neighbours(NodeId node, std::string_view type) const;

  const std::string& name(NodeId node) const { return nodes_[node].name; }
  size_t node_count() const { return nodes_.size(); }
  size_t relationship_count() const { return relationships_.size(); }

 private:
  std::vector<Node> nodes_;
  std::vector<Relationship> relationships_;
  std::unordered_map<std::string, NodeId> ids_;
};

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_GRAPH_H_
