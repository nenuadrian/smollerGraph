#ifndef SMOLLERGRAPH_RECORD_FILE_H_
#define SMOLLERGRAPH_RECORD_FILE_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

#include "absl/status/status.h"
#include "buffer_pool.h"
#include "pager.h"

namespace smollergraph {

// A file of fixed-size records, packed into pages after a header page, which
// holds how many records there are. A record's id says which page it's on and
// where, so finding one needs no search. Its pages go through a buffer pool,
// which writes the changed ones to the file when it needs their frames, or
// when it's flushed or closed.
template <typename Record>
class RecordFile {
  static_assert(std::is_trivially_copyable_v<Record>,
                "records are copied to and from pages as bytes");

 public:
  static constexpr uint32_t kPerPage = kPageSize / sizeof(Record);
  // The most pages the pool keeps in memory, 64 MiB of them, which holds the
  // million relationships we benchmark.
  static constexpr size_t kPoolPages = 16384;

  // Opening another file writes back the changes to the one that was open.
  absl::Status Open(const std::string& path) {
    pool_.Clear();
    if (absl::Status status = pager_.Open(path); !status.ok()) {
      return status;
    }
    LoadCount();
    return absl::OkStatus();
  }
  absl::Status OpenTemporary() {
    pool_.Clear();
    if (absl::Status status = pager_.OpenTemporary(); !status.ok()) {
      return status;
    }
    LoadCount();
    return absl::OkStatus();
  }
  // Writes the changed pages to the file.
  void Flush() { pool_.Flush(); }

  uint32_t count() const { return count_; }
  uint64_t page_reads() const { return pager_.reads(); }
  uint64_t page_writes() const { return pager_.writes(); }

  // Page 0 is the header, so the records start on page 1.
  static PageId PageOf(uint32_t id) { return 1 + id / kPerPage; }
  static size_t OffsetOf(uint32_t id) {
    return (id % kPerPage) * sizeof(Record);
  }

  // Reads record `id`, which has to be below count().
  Record Read(uint32_t id) const {
    PageHandle page = pool_.Fetch(PageOf(id));
    Record record;
    std::memcpy(&record, page.read().data() + OffsetOf(id), sizeof(Record));
    return record;
  }

  void Write(uint32_t id, const Record& record) {
    PageHandle page = pool_.Fetch(PageOf(id));
    std::memcpy(page.write().data() + OffsetOf(id), &record, sizeof(Record));
  }

  // Adds a record after the last one, and returns its id.
  uint32_t Append(const Record& record) {
    uint32_t id = count_;
    Write(id, record);
    count_ = id + 1;
    PageHandle header = pool_.Fetch(0);
    std::memcpy(header.write().data(), &count_, sizeof(count_));
    return id;
  }

 private:
  void LoadCount() {
    count_ = 0;
    if (pager_.page_count() == 0) return;
    PageHandle header = pool_.Fetch(0);
    std::memcpy(&count_, header.read().data(), sizeof(count_));
  }

  // The pool is declared after the pager, so it's destroyed first, and
  // writes its changed pages while the file is still open. It's mutable, as
  // Read is const and still brings pages into the pool.
  Pager pager_;
  mutable BufferPool pool_{pager_, kPoolPages};
  uint32_t count_ = 0;
};

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_RECORD_FILE_H_
