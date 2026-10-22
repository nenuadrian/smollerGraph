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

}  // namespace

Graph::Graph() {
  absl::Status status = nodes_.OpenTemporary();
  if (status.ok()) status = relationships_.OpenTemporary();
  if (status.ok()) status = names_.OpenTemporary();
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
  if (status.ok()) status = names_.Open(path + "/names");
  return status;
}

NodeId Graph::AddNode(std::string_view name) {
  if (std::optional<NodeId> found = Find(name)) return *found;
  NodeRecord record;
  CopyField(record.name, name);
  record.first_out = kNoRelationship;
  NodeId id = nodes_.Append(record);
  names_.Insert(name, id);
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
  return names_.Find(name);
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
