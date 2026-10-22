#include "name_index.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

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
  while (!node.leaf) node = Load(Child(node, name));
  uint32_t at = Position(node, name);
  if (at < node.count && NameOf(node.entries[at]) == name) {
    return node.entries[at].value;
  }
  return std::nullopt;
}

void NameIndex::Insert(std::string_view name, uint32_t id) {
  Entry entry{};
  std::memcpy(entry.name, name.data(), std::min(name.size(), kMaxName));
  entry.value = id;
  std::optional<Entry> split = InsertUnder(root_, entry);
  if (split) {
    // The root split, so a new root goes above its two halves, and the tree
    // is a level taller.
    Node root{};
    root.first = root_;
    root.entries[0] = *split;
    root.count = 1;
    root_ = pages_++;
    Store(root_, root);
  }
  StoreHeader();
}

std::optional<NameIndex::Entry> NameIndex::InsertUnder(PageId id,
                                                       const Entry& entry) {
  Node node = Load(id);
  if (node.leaf) return Add(id, node, Position(node, NameOf(entry)), entry);
  std::optional<Entry> split = InsertUnder(Child(node, NameOf(entry)), entry);
  if (!split) return std::nullopt;
  return Add(id, node, Position(node, NameOf(*split)), *split);
}

std::optional<NameIndex::Entry> NameIndex::Add(PageId id, Node& node,
                                               uint32_t at,
                                               const Entry& entry) {
  std::vector<Entry> entries(node.entries, node.entries + node.count);
  entries.insert(entries.begin() + at, entry);
  if (entries.size() <= kEntries) {
    std::copy(entries.begin(), entries.end(), node.entries);
    node.count = static_cast<uint32_t>(entries.size());
    Store(id, node);
    return std::nullopt;
  }
  // The page is full, so it keeps the first half, and a new page on its right
  // takes the rest.
  size_t half = entries.size() / 2;
  Node right{};
  right.leaf = node.leaf;
  PageId right_id = pages_++;
  Entry up = entries[half];
  up.value = right_id;
  if (node.leaf) {
    std::copy(entries.begin() + half, entries.end(), right.entries);
    right.count = static_cast<uint32_t>(entries.size() - half);
  } else {
    // Above the leaves, the middle entry moves up, and its page becomes the
    // first of the right half.
    right.first = entries[half].value;
    std::copy(entries.begin() + half + 1, entries.end(), right.entries);
    right.count = static_cast<uint32_t>(entries.size() - half - 1);
  }
  std::copy(entries.begin(), entries.begin() + half, node.entries);
  node.count = static_cast<uint32_t>(half);
  Store(id, node);
  Store(right_id, right);
  return up;
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

PageId NameIndex::Child(const Node& node, std::string_view name) {
  // An entry's page holds the names from its name on, so `name` is under the
  // last entry whose name isn't after it, or under `first`, when they all are.
  const Entry* end = node.entries + node.count;
  const Entry* after = std::upper_bound(
      node.entries, end, name, [](std::string_view name, const Entry& entry) {
        return name < NameOf(entry);
      });
  return after == node.entries ? node.first : (after - 1)->value;
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

void NameIndex::StoreHeader() {
  Header header{root_, pages_};
  PageHandle page = pool_.Fetch(0);
  std::memcpy(page.write().data(), &header, sizeof(header));
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
