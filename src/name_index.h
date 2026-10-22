#ifndef SMOLLERGRAPH_NAME_INDEX_H_
#define SMOLLERGRAPH_NAME_INDEX_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "buffer_pool.h"
#include "pager.h"

namespace smollergraph {

// An index from names to ids, kept in a file of pages as a B+ tree: leaves of
// names in order, each with its id, and pages above them that say which page
// holds which names. Finding a name reads a page on each level.
class NameIndex {
 public:
  // The longest name it keeps, as long as a node's.
  static constexpr size_t kMaxName = 27;

  // Opens the index in the file at `path`, creating it when it isn't there.
  absl::Status Open(const std::string& path);
  // Opens an index in a file with no name, which is deleted once it's closed.
  absl::Status OpenTemporary();
  // Writes the changed pages to the file.
  void Flush() { pool_.Flush(); }
  uint64_t page_reads() const { return pager_.reads(); }
  uint64_t page_writes() const { return pager_.writes(); }

  // Returns the id kept for `name`, if there's one.
  std::optional<uint32_t> Find(std::string_view name) const;
  // Keeps `id` for `name`, which the index doesn't have yet.
  void Insert(std::string_view name, uint32_t id);

 private:
  // A name, padded with zeros, and the id kept for it. Above the leaves, the
  // id is the page that holds the names from that one on.
  struct Entry {
    char name[kMaxName + 1];
    uint32_t value;
  };
  static constexpr size_t kHeaderSize = 16;
  static constexpr uint32_t kEntries =
      (kPageSize - kHeaderSize) / sizeof(Entry);

  // A page of the tree, as it's laid out in the file: a header of 16 bytes,
  // then its entries, in name order. Above the leaves, `first` is the page for
  // names before the first entry's.
  struct Node {
    uint32_t leaf;
    uint32_t count;
    PageId first;
    uint32_t unused;
    Entry entries[kEntries];
  };
  static_assert(sizeof(Node) <= kPageSize);

  // Page 0 says which page is the root, and how many pages there are.
  struct Header {
    PageId root;
    PageId pages;
  };

  // The name in an entry, up to its first zero.
  static std::string_view NameOf(const Entry& entry);
  // How many of a page's entries come before `name`, which is where `name` is,
  // or where it would go.
  static uint32_t Position(const Node& node, std::string_view name);
  // The page below `node` that holds `name`, or would.
  static PageId Child(const Node& node, std::string_view name);

  // Inserts `entry` under page `id`. When a page splits, returns the entry that
  // the page above needs, for the new page.
  std::optional<Entry> InsertUnder(PageId id, const Entry& entry);
  // Puts `entry` in `node`, page `id`, at `at`, splitting the page when it's
  // full.
  std::optional<Entry> Add(PageId id, Node& node, uint32_t at,
                           const Entry& entry);
  void StoreHeader();

  void Start();
  Node Load(PageId id) const;
  void Store(PageId id, const Node& node);

  // Up to 16 MiB of the index in memory, about 150,000 names.
  static constexpr size_t kPoolPages = 4096;

  Pager pager_;
  mutable BufferPool pool_{pager_, kPoolPages};
  PageId root_ = 0;
  PageId pages_ = 0;
};

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_NAME_INDEX_H_
