#ifndef SMOLLERGRAPH_GRAPH_H_
#define SMOLLERGRAPH_GRAPH_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "name_index.h"
#include "record_file.h"

namespace smollergraph {

using NodeId = uint32_t;
using RelationshipId = uint32_t;

// The id at the end of a chain, where there's no next relationship.
inline constexpr RelationshipId kNoRelationship = UINT32_MAX;

// A node on disk: its name, and the first of the relationships that start at
// it.
struct NodeRecord {
  char name[28];
  RelationshipId first_out;
};

// A relationship on disk: its type, the nodes it joins, and the next
// relationship that starts at `from`, so a node's relationships form a chain.
struct RelationshipRecord {
  char type[20];
  NodeId from;
  NodeId to;
  RelationshipId next_out;
};

static_assert(sizeof(NodeRecord) == 32);
static_assert(sizeof(RelationshipRecord) == 32);

// A graph kept in two files of records, one for nodes and one for
// relationships, with an index that finds a node by its name.
class Graph {
 public:
  // The longest name and relationship type that fit in their records.
  static constexpr size_t kMaxName = sizeof(NodeRecord::name) - 1;
  static constexpr size_t kMaxType = sizeof(RelationshipRecord::type) - 1;

  // A graph in temporary files, which are deleted with it.
  Graph();
  // Opens the graph in the folder at `path`, creating it when it isn't there.
  absl::Status Open(const std::string& path);

  // Writes the graph's changed pages to its files, which closing it does too.
  void Flush() {
    nodes_.Flush();
    relationships_.Flush();
    names_.Flush();
  }

  // Returns the node with this name, adding it the first time. The name has to
  // be at most kMaxName bytes long.
  NodeId AddNode(std::string_view name);
  void AddRelationship(NodeId from, std::string_view type, NodeId to);

  // Returns the node with this name, if the graph has one.
  std::optional<NodeId> Find(std::string_view name) const;

  // Returns the nodes one hop from `node` along relationships of `type`.
  std::vector<NodeId> Neighbours(NodeId node, std::string_view type) const;

  std::string name(NodeId node) const { return nodes_.Read(node).name; }
  NodeRecord node(NodeId id) const { return nodes_.Read(id); }
  RelationshipRecord relationship(RelationshipId id) const {
    return relationships_.Read(id);
  }
  size_t node_count() const { return nodes_.count(); }
  size_t relationship_count() const { return relationships_.count(); }
  // How many pages the graph's files have read and written.
  uint64_t page_reads() const {
    return nodes_.page_reads() + relationships_.page_reads() +
           names_.page_reads();
  }
  uint64_t page_writes() const {
    return nodes_.page_writes() + relationships_.page_writes() +
           names_.page_writes();
  }

 private:
  RecordFile<NodeRecord> nodes_;
  RecordFile<RelationshipRecord> relationships_;
  // Names to node ids, in a file of their own.
  NameIndex names_;
};

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_GRAPH_H_
