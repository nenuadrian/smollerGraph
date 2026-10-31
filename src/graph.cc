#include "graph.h"

#include <sys/stat.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

namespace smollergraph {
namespace {

// Copies `text` into a field of a record, padded with zeros, and cut short if
// it's too long, so the field always ends in a zero.
template <size_t N>
void CopyField(char (&field)[N], std::string_view text) {
  std::memset(field, 0, N);
  std::memcpy(field, text.data(), std::min(text.size(), N - 1));
}

// Puts a value in a property's record, with its type, so it can be read back.
void Encode(const Value& value, PropertyRecord& record) {
  if (const int64_t* number = std::get_if<int64_t>(&value)) {
    record.type = ValueType::kInteger;
    std::memset(record.value, 0, sizeof(record.value));
    std::memcpy(record.value, number, sizeof(*number));
  } else {
    record.type = ValueType::kString;
    CopyField(record.value, std::get<std::string>(value));
  }
}

}  // namespace

Value ValueOf(const PropertyRecord& record) {
  if (record.type == ValueType::kInteger) {
    int64_t number = 0;
    std::memcpy(&number, record.value, sizeof(number));
    return number;
  }
  return std::string(record.value);
}

Graph::Graph() {
  absl::Status status = nodes_.OpenTemporary();
  if (status.ok()) status = relationships_.OpenTemporary();
  if (status.ok()) status = properties_.OpenTemporary();
  if (status.ok()) status = names_.OpenTemporary();
  if (status.ok()) status = labels_.OpenTemporary();
  if (status.ok()) status = types_.OpenTemporary();
  if (status.ok()) status = embeddings_.OpenTemporary();
  if (!status.ok()) {
    std::fprintf(stderr, "smollergraph: no temporary files: %s\n",
                 status.ToString().c_str());
    std::abort();
  }
}

absl::Status Graph::Open(const std::string& path) {
  // A folder that's there already, from an earlier run, is fine.
  if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
    return absl::ErrnoToStatus(errno, absl::StrCat("can't make ", path));
  }
  // Each file opens if the one before it did, and the first error is the one
  // we return.
  absl::Status status = nodes_.Open(path + "/nodes");
  if (status.ok()) status = relationships_.Open(path + "/relationships");
  if (status.ok()) status = properties_.Open(path + "/properties");
  if (status.ok()) status = names_.Open(path + "/names");
  if (status.ok()) status = labels_.Open(path + "/labels");
  if (status.ok()) status = types_.Open(path + "/types");
  if (status.ok()) status = embeddings_.Open(path + "/embeddings");
  return status;
}

NodeId Graph::AddNode(std::string_view name) {
  if (std::optional<NodeId> found = Find(name)) return *found;
  NodeRecord record;
  CopyField(record.name, name);
  record.label = kNoToken;
  record.first_out = kNoRelationship;
  record.first_in = kNoRelationship;
  record.first_property = kNoProperty;
  NodeId id = nodes_.Append(record);
  names_.Insert(name, id);
  return id;
}

RelationshipId Graph::AddRelationship(NodeId from, std::string_view type,
                                      NodeId to) {
  RelationshipId id = relationships_.count();
  RelationshipRecord record;
  record.type = types_.Add(type);
  record.from = from;
  record.to = to;
  record.first_property = kNoProperty;
  // The new relationship goes at the front of two chains, the out chain of
  // the node it starts at, and the in chain of the node it ends at.
  NodeRecord start = nodes_.Read(from);
  record.next_out = start.first_out;
  start.first_out = id;
  nodes_.Write(from, start);
  // The end is read after the start is written, as they're one node when a
  // relationship goes from a node to itself.
  NodeRecord end = nodes_.Read(to);
  record.next_in = end.first_in;
  end.first_in = id;
  nodes_.Write(to, end);
  relationships_.Append(record);
  return id;
}

void Graph::SetLabel(NodeId node, std::string_view label) {
  NodeRecord record = nodes_.Read(node);
  record.label = labels_.Add(label);
  nodes_.Write(node, record);
}

void Graph::SetProperty(NodeId node, std::string_view key, const Value& value) {
  NodeRecord record = nodes_.Read(node);
  Set(record.first_property, key, value);
  nodes_.Write(node, record);
}

void Graph::SetRelationshipProperty(RelationshipId relationship,
                                    std::string_view key, const Value& value) {
  RelationshipRecord record = relationships_.Read(relationship);
  Set(record.first_property, key, value);
  relationships_.Write(relationship, record);
}

void Graph::Set(PropertyId& first, std::string_view key, const Value& value) {
  PropertyId id = first;
  while (id != kNoProperty) {
    PropertyRecord property = properties_.Read(id);
    if (key == property.key) {
      Encode(value, property);
      properties_.Write(id, property);
      return;
    }
    id = property.next;
  }
  // Like a relationship, a new property goes at the front of the chain.
  PropertyRecord property;
  CopyField(property.key, key);
  Encode(value, property);
  property.next = first;
  first = properties_.Append(property);
}

void Graph::SetEmbedding(NodeId node, const Embedding& embedding) {
  if (node < embeddings_.count()) {
    embeddings_.Write(node, embedding);
  } else {
    embeddings_.Append(embedding);
  }
}

std::vector<Embedding> Graph::Embeddings() const {
  std::vector<Embedding> embeddings;
  if (embeddings_.count() != nodes_.count()) return embeddings;
  embeddings.reserve(embeddings_.count());
  for (NodeId node = 0; node < embeddings_.count(); ++node) {
    embeddings.push_back(embeddings_.Read(node));
  }
  return embeddings;
}

std::optional<Value> Graph::FindProperty(NodeId node,
                                         std::string_view key) const {
  NodeRecord record = nodes_.Read(node);
  if (key == "name") return std::string(record.name);
  return Lookup(record.first_property, key);
}

std::optional<Value> Graph::FindRelationshipProperty(
    RelationshipId relationship, std::string_view key) const {
  return Lookup(relationships_.Read(relationship).first_property, key);
}

std::optional<Value> Graph::Lookup(PropertyId first,
                                   std::string_view key) const {
  PropertyId id = first;
  while (id != kNoProperty) {
    PropertyRecord property = properties_.Read(id);
    if (key == property.key) return ValueOf(property);
    id = property.next;
  }
  return std::nullopt;
}

std::vector<Property> Graph::Properties(NodeId node) const {
  return Chain(nodes_.Read(node).first_property);
}

std::vector<Property> Graph::RelationshipProperties(
    RelationshipId relationship) const {
  return Chain(relationships_.Read(relationship).first_property);
}

std::vector<Property> Graph::Chain(PropertyId first) const {
  std::vector<Property> found;
  PropertyId id = first;
  while (id != kNoProperty) {
    PropertyRecord property = properties_.Read(id);
    found.push_back({property.key, ValueOf(property)});
    id = property.next;
  }
  // Newest first in the chain, and oldest first for callers.
  std::reverse(found.begin(), found.end());
  return found;
}

std::optional<NodeId> Graph::Find(std::string_view name) const {
  return names_.Find(name);
}

std::vector<Hop> Graph::Hops(NodeId node, std::string_view type,
                             Direction direction) const {
  std::vector<Hop> hops;
  // Without a token for the type, no relationship has it, so there are none.
  std::optional<TokenId> token = types_.Find(type);
  if (!token) return hops;
  NodeRecord record = nodes_.Read(node);
  if (direction != Direction::kIn) {
    Follow(record.first_out, true, *token, hops);
  }
  if (direction != Direction::kOut) {
    Follow(record.first_in, false, *token, hops);
  }
  return hops;
}

void Graph::Follow(RelationshipId first, bool out, TokenId type,
                   std::vector<Hop>& hops) const {
  std::vector<Hop> chain;
  RelationshipId id = first;
  while (id != kNoRelationship) {
    RelationshipRecord record = relationships_.Read(id);
    if (record.type == type) {
      chain.push_back({id, out ? record.to : record.from});
    }
    id = out ? record.next_out : record.next_in;
  }
  // The chain is newest first, and callers get them in the order they came.
  hops.insert(hops.end(), chain.rbegin(), chain.rend());
}

std::vector<NodeId> Graph::Neighbours(NodeId node, std::string_view type,
                                      Direction direction) const {
  std::vector<NodeId> found;
  for (Hop hop : Hops(node, type, direction)) found.push_back(hop.node);
  return found;
}

}  // namespace smollergraph
