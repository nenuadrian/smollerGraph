#ifndef SMOLLERGRAPH_RECORD_FILE_H_
#define SMOLLERGRAPH_RECORD_FILE_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

#include "absl/status/status.h"
#include "pager.h"

namespace smollergraph {

// A file of fixed-size records, packed into pages after a header page, which
// holds how many records there are. A record's id says which page it's on and
// where, so finding one needs no search.
template <typename Record>
class RecordFile {
  static_assert(std::is_trivially_copyable_v<Record>,
                "records are copied to and from pages as bytes");

 public:
  static constexpr uint32_t kPerPage = kPageSize / sizeof(Record);

  absl::Status Open(const std::string& path) {
    if (absl::Status status = pager_.Open(path); !status.ok()) {
      return status;
    }
    LoadCount();
    return absl::OkStatus();
  }
  absl::Status OpenTemporary() {
    if (absl::Status status = pager_.OpenTemporary(); !status.ok()) {
      return status;
    }
    LoadCount();
    return absl::OkStatus();
  }

  uint32_t count() const { return count_; }

  // Reads record `id`, which has to be below count().
  Record Read(uint32_t id) const {
    Page page;
    pager_.Read(PageOf(id), page);
    Record record;
    std::memcpy(&record, page.data() + OffsetOf(id), sizeof(Record));
    return record;
  }

  void Write(uint32_t id, const Record& record) {
    Page page{};
    if (PageOf(id) < pager_.page_count()) pager_.Read(PageOf(id), page);
    std::memcpy(page.data() + OffsetOf(id), &record, sizeof(Record));
    pager_.Write(PageOf(id), page);
  }

  // Adds a record after the last one, and returns its id.
  uint32_t Append(const Record& record) {
    uint32_t id = count_;
    Write(id, record);
    count_ = id + 1;
    Page header{};
    std::memcpy(header.data(), &count_, sizeof(count_));
    pager_.Write(0, header);
    return id;
  }

 private:
  // Page 0 is the header, so the records start on page 1.
  static PageId PageOf(uint32_t id) { return 1 + id / kPerPage; }
  static size_t OffsetOf(uint32_t id) {
    return (id % kPerPage) * sizeof(Record);
  }

  void LoadCount() {
    count_ = 0;
    if (pager_.page_count() == 0) return;
    Page header;
    pager_.Read(0, header);
    std::memcpy(&count_, header.data(), sizeof(count_));
  }

  Pager pager_;
  uint32_t count_ = 0;
};

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_RECORD_FILE_H_
