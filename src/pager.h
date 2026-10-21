#ifndef SMOLLERGRAPH_PAGER_H_
#define SMOLLERGRAPH_PAGER_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace smollergraph {

// What the database reads and writes at a time: 4 KiB, the size of a block on
// most file systems and disks.
inline constexpr size_t kPageSize = 4096;

using PageId = uint32_t;
using Page = std::array<std::byte, kPageSize>;

// A file of pages, each read and written whole, by its number.
class Pager {
 public:
  Pager() = default;
  ~Pager();
  Pager(const Pager&) = delete;
  Pager& operator=(const Pager&) = delete;

  // Opens the file at `path`, creating it when it isn't there. Returns false,
  // with the reason in `error`, when it can't.
  bool Open(const std::string& path, std::string& error);
  // Opens a file with no name, which is deleted once it's closed.
  bool OpenTemporary(std::string& error);

  PageId page_count() const { return page_count_; }
  // Reads page `id`, which has to be below page_count().
  void Read(PageId id, Page& page) const;
  // Writes page `id`. Writing the page at page_count() adds it to the file.
  void Write(PageId id, const Page& page);

 private:
  bool Adopt(int fd, std::string& error);

  int fd_ = -1;
  PageId page_count_ = 0;
};

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_PAGER_H_
