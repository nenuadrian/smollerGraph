#ifndef SMOLLERGRAPH_GRAPH_H_
#define SMOLLERGRAPH_GRAPH_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "name_index.h"
#include "record_file.h"

namespace smollergraph {

using NodeId = uint32_t;
using RelationshipId = uint32_t;
using PropertyId = uint32_t;

// The ids at the end of a chain, where there's no next relationship, or no
// next property.
inline constexpr RelationshipId kNoRelationship = UINT32_MAX;
inline constexpr PropertyId kNoProperty = UINT32_MAX;

// A node on disk: its name, its label, which says what kind of node it is,
// the first of the relationships that start at it, and the first of its
// properties.
struct NodeRecord {
  char name[44];
  char label[12];
  RelationshipId first_out;
  PropertyId first_property;
};

// A relationship on disk: its type, the nodes it joins, and the next
// relationship that starts at `from`, so a node's relationships form a chain,
// then the first of its properties.
struct RelationshipRecord {
  char type[16];
  NodeId from;
  NodeId to;
  RelationshipId next_out;
  PropertyId first_property;
};

enum class ValueType : uint8_t { kInteger, kString };

// A property on disk: its key, its value, and the next property of the node or
// relationship it belongs to, so their properties form a chain too. A whole
// number takes the first 8 bytes of the value, and a string runs to its zero.
struct PropertyRecord {
  char key[16];
  ValueType type;
  char value[43];
  PropertyId next;
};

// A node and a property take 64 bytes, room for the names and values of a real
// graph, and a relationship still takes 32.
static_assert(sizeof(NodeRecord) == 64);
static_assert(sizeof(RelationshipRecord) == 32);
static_assert(sizeof(PropertyRecord) == 64);

// A property's value, as the graph hands it out: a whole number or a string.
using Value = std::variant<int64_t, std::string>;

struct Property {
  std::string key;
  Value value;
};

// Reads the value a property's record holds.
Value ValueOf(const PropertyRecord& record);

// A graph kept in three files of records, one for nodes, one for
// relationships and one for their properties, with an index that finds a node
// by its name.
class Graph {
 public:
  // The longest name, label and relationship type that fit in their records.
  static constexpr size_t kMaxName = sizeof(NodeRecord::name) - 1;
  static constexpr size_t kMaxLabel = sizeof(NodeRecord::label) - 1;
  static constexpr size_t kMaxType = sizeof(RelationshipRecord::type) - 1;
  // The longest key and string that fit in a property's record.
  static constexpr size_t kMaxKey = sizeof(PropertyRecord::key) - 1;
  static constexpr size_t kMaxString = sizeof(PropertyRecord::value) - 1;

  // A graph in temporary files, which are deleted with it.
  Graph();
  // Opens the graph in the folder at `path`, creating it when it isn't there.
  absl::Status Open(const std::string& path);

  // Writes the graph's changed pages to its files, which closing it does too.
  void Flush() {
    nodes_.Flush();
    relationships_.Flush();
    properties_.Flush();
    names_.Flush();
  }

  // Returns the node with this name, adding it the first time. The name has to
  // be at most kMaxName bytes long.
  NodeId AddNode(std::string_view name);
  RelationshipId AddRelationship(NodeId from, std::string_view type, NodeId to);
  // Gives a node its label, such as Species, in place of the one it had. The
  // label has to be at most kMaxLabel bytes long.
  void SetLabel(NodeId node, std::string_view label);

  // Sets a property of a node or a relationship, replacing the value it had.
  // The key has to be at most kMaxKey bytes long, and a string kMaxString. A
  // node's name is in its record already, so it's not a key to set.
  void SetProperty(NodeId node, std::string_view key, const Value& value);
  void SetRelationshipProperty(RelationshipId relationship,
                               std::string_view key, const Value& value);

  // Returns a node's property, if it has one. Its name is a property too.
  std::optional<Value> FindProperty(NodeId node, std::string_view key) const;
  // Returns the properties of a node or a relationship, in the order they
  // were first set.
  std::vector<Property> Properties(NodeId node) const;
  std::vector<Property> RelationshipProperties(
      RelationshipId relationship) const;

  // Returns the node with this name, if the graph has one.
  std::optional<NodeId> Find(std::string_view name) const;

  // Returns up to `limit` names that start with `prefix`, in order.
  std::vector<std::string> NamesStartingWith(std::string_view prefix,
                                             size_t limit) const {
    return names_.NamesStartingWith(prefix, limit);
  }

  // Returns the nodes one hop from `node` along relationships of `type`.
  std::vector<NodeId> Neighbours(NodeId node, std::string_view type) const;

  std::string name(NodeId node) const { return nodes_.Read(node).name; }
  std::string label(NodeId node) const { return nodes_.Read(node).label; }
  NodeRecord node(NodeId id) const { return nodes_.Read(id); }
  RelationshipRecord relationship(RelationshipId id) const {
    return relationships_.Read(id);
  }
  PropertyRecord property(PropertyId id) const { return properties_.Read(id); }
  size_t node_count() const { return nodes_.count(); }
  size_t relationship_count() const { return relationships_.count(); }
  // How many pages the graph's files have read and written.
  uint64_t page_reads() const {
    return nodes_.page_reads() + relationships_.page_reads() +
           properties_.page_reads() + names_.page_reads();
  }
  uint64_t page_writes() const {
    return nodes_.page_writes() + relationships_.page_writes() +
           properties_.page_writes() + names_.page_writes();
  }

 private:
  // Sets `key` in the chain of properties that starts at `first`. A key the
  // chain doesn't have yet gets a record of its own, at the front.
  void Set(PropertyId& first, std::string_view key, const Value& value);
  std::vector<Property> Chain(PropertyId first) const;

  RecordFile<NodeRecord> nodes_;
  RecordFile<RelationshipRecord> relationships_;
  RecordFile<PropertyRecord> properties_;
  // Names to node ids, in a file of their own.
  NameIndex names_;
};

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_GRAPH_H_
