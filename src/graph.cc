#include "graph.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <system_error>

namespace smollergraph {
namespace {

// Copies `text` into a field of a record, padded with zeros, and cut short if
// it's too long, so the field always ends in a zero.
template <size_t N>
void CopyField(char (&field)[N], std::string_view text) {
  std::memset(field, 0, N);
  std::memcpy(field, text.data(), std::min(text.size(), N - 1));
}

}  // namespace

Graph::Graph() {
  std::string error;
  if (!nodes_.OpenTemporary(error) || !relationships_.OpenTemporary(error)) {
    std::fprintf(stderr, "smollergraph: no temporary files: %s\n",
                 error.c_str());
    std::abort();
  }
}

bool Graph::Open(const std::string& path, std::string& error) {
  std::error_code failed;
  std::filesystem::create_directories(path, failed);
  if (failed) {
    error = failed.message();
    return false;
  }
  if (!nodes_.Open(path + "/nodes", error) ||
      !relationships_.Open(path + "/relationships", error)) {
    return false;
  }
  // There's no index on names yet, so we read each node to find them again.
  ids_.clear();
  for (NodeId id = 0; id < nodes_.count(); ++id) {
    ids_.emplace(nodes_.Read(id).name, id);
  }
  return true;
}

NodeId Graph::AddNode(std::string_view name) {
  if (std::optional<NodeId> found = Find(name)) return *found;
  NodeRecord record;
  CopyField(record.name, name);
  record.first_out = kNoRelationship;
  NodeId id = nodes_.Append(record);
  ids_.emplace(std::string(name), id);
  return id;
}

void Graph::AddRelationship(NodeId from, std::string_view type, NodeId to) {
  NodeRecord node = nodes_.Read(from);
  RelationshipRecord record;
  CopyField(record.type, type);
  record.from = from;
  record.to = to;
  // The new relationship goes at the front of the chain, so adding one takes
  // two writes, however long the chain is.
  record.next_out = node.first_out;
  node.first_out = relationships_.Append(record);
  nodes_.Write(from, node);
}

std::optional<NodeId> Graph::Find(std::string_view name) const {
  auto it = ids_.find(std::string(name));
  if (it == ids_.end()) return std::nullopt;
  return it->second;
}

std::vector<NodeId> Graph::Neighbours(NodeId node,
                                      std::string_view type) const {
  std::vector<NodeId> found;
  RelationshipId id = nodes_.Read(node).first_out;
  while (id != kNoRelationship) {
    RelationshipRecord record = relationships_.Read(id);
    if (type == record.type) found.push_back(record.to);
    id = record.next_out;
  }
  // The chain is newest first, and callers get them in the order they came.
  std::reverse(found.begin(), found.end());
  return found;
}

}  // namespace smollergraph
