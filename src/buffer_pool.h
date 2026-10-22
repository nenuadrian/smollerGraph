#ifndef SMOLLERGRAPH_BUFFER_POOL_H_
#define SMOLLERGRAPH_BUFFER_POOL_H_

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "pager.h"

namespace smollergraph {

class BufferPool;

// A page pinned in a buffer pool. The pool doesn't evict a pinned page, and
// the handle unpins it when it goes away.
class PageHandle {
 public:
  PageHandle(BufferPool& pool, size_t frame) : pool_(pool), frame_(frame) {}
  ~PageHandle();
  PageHandle(const PageHandle&) = delete;
  PageHandle& operator=(const PageHandle&) = delete;

  // The page, to read.
  const Page& read() const;
  // The page, to change, which marks it to be written back to the file.
  Page& write();

 private:
  BufferPool& pool_;
  size_t frame_;
};

// Keeps some of a file's pages in memory, in frames, so reading or changing a
// page there needs no system call. The file is read when a page comes into a
// frame, and written when a changed page leaves one, or when it's flushed.
class BufferPool {
 public:
  // A pool of up to `frames` pages of the file `pager` has open.
  BufferPool(Pager& pager, size_t frames);
  // Writes the changed pages back.
  ~BufferPool();
  BufferPool(const BufferPool&) = delete;
  BufferPool& operator=(const BufferPool&) = delete;

  // Pins page `id`, reading it from the file when it isn't in a frame. A page
  // past the end of the file starts out as zeros.
  PageHandle Fetch(PageId id);
  // Writes the changed pages back to the file, and keeps them.
  void Flush();
  // Writes the changed pages back, and forgets them all, before the pager
  // opens another file.
  void Clear();

 private:
  friend class PageHandle;

  // A frame holds a page while it's in the pool. `referenced` is set each time
  // the page is fetched, and the clock clears it on its way round.
  struct Frame {
    Page page;
    PageId id = 0;
    int pins = 0;
    bool dirty = false;
    bool referenced = false;
  };

  // Picks a frame to reuse, by the clock: a page that hasn't been fetched since
  // the hand last passed it.
  size_t Victim();
  void Unpin(size_t frame);

  Pager& pager_;
  size_t capacity_;
  std::vector<Frame> frames_;
  // Which frame holds each page in the pool.
  std::unordered_map<PageId, size_t> table_;
  size_t hand_ = 0;
};

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_BUFFER_POOL_H_
