#include "name_index.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace smollergraph {

absl::Status NameIndex::Open(const std::string& path) {
  pool_.Clear();
  if (absl::Status status = pager_.Open(path); !status.ok()) {
    return status;
  }
  Start();
  return absl::OkStatus();
}

absl::Status NameIndex::OpenTemporary() {
  pool_.Clear();
  if (absl::Status status = pager_.OpenTemporary(); !status.ok()) {
    return status;
  }
  Start();
  return absl::OkStatus();
}

std::optional<uint32_t> NameIndex::Find(std::string_view name) const {
  Node node = Load(root_);
  uint32_t at = Position(node, name);
  if (at < node.count && NameOf(node.entries[at]) == name) {
    return node.entries[at].value;
  }
  return std::nullopt;
}

void NameIndex::Insert(std::string_view name, uint32_t id) {
  Node node = Load(root_);
  if (node.count == kEntries) {
    std::fprintf(stderr, "smollergraph: the name index's page is full\n");
    std::abort();
  }
  uint32_t at = Position(node, name);
  // The entries from `at` on move along one, to make room.
  std::copy_backward(node.entries + at, node.entries + node.count,
                     node.entries + node.count + 1);
  Entry& entry = node.entries[at];
  std::memset(entry.name, 0, sizeof(entry.name));
  std::memcpy(entry.name, name.data(), std::min(name.size(), kMaxName));
  entry.value = id;
  ++node.count;
  Store(root_, node);
}

std::string_view NameIndex::NameOf(const Entry& entry) {
  return std::string_view(entry.name, strnlen(entry.name, sizeof(entry.name)));
}

uint32_t NameIndex::Position(const Node& node, std::string_view name) {
  const Entry* end = node.entries + node.count;
  const Entry* found = std::lower_bound(
      node.entries, end, name, [](const Entry& entry, std::string_view name) {
        return NameOf(entry) < name;
      });
  return static_cast<uint32_t>(found - node.entries);
}

// A new file gets a header and an empty leaf as its root. A file that has them
// says where its root is.
void NameIndex::Start() {
  Header header{1, 2};
  if (pager_.page_count() == 0) {
    Node root{};
    root.leaf = 1;
    Store(header.root, root);
    PageHandle page = pool_.Fetch(0);
    std::memcpy(page.write().data(), &header, sizeof(header));
  } else {
    PageHandle page = pool_.Fetch(0);
    std::memcpy(&header, page.read().data(), sizeof(header));
  }
  root_ = header.root;
  pages_ = header.pages;
}

// A page is copied in and out of its frame whole, as a Node, which is safer
// than pointing into the frame's bytes as if they were one.
NameIndex::Node NameIndex::Load(PageId id) const {
  PageHandle page = pool_.Fetch(id);
  Node node;
  std::memcpy(&node, page.read().data(), sizeof(node));
  return node;
}

void NameIndex::Store(PageId id, const Node& node) {
  PageHandle page = pool_.Fetch(id);
  std::memcpy(page.write().data(), &node, sizeof(node));
}

}  // namespace smollergraph
