#include "buffer_pool.h"

#include <cstdio>
#include <cstdlib>

namespace smollergraph {

PageHandle::~PageHandle() { pool_.Unpin(frame_); }

const Page& PageHandle::read() const { return pool_.frames_[frame_].page; }

Page& PageHandle::write() {
  BufferPool::Frame& frame = pool_.frames_[frame_];
  frame.dirty = true;
  return frame.page;
}

BufferPool::BufferPool(Pager& pager, size_t frames)
    : pager_(pager), capacity_(frames) {
  // Frames are made as pages come in, so a pool that doesn't fill up stays
  // small.
  frames_.reserve(capacity_);
}

BufferPool::~BufferPool() { Flush(); }

PageHandle BufferPool::Fetch(PageId id) {
  auto found = table_.find(id);
  if (found != table_.end()) {
    Frame& frame = frames_[found->second];
    ++frame.pins;
    frame.referenced = true;
    return PageHandle(*this, found->second);
  }

  size_t at;
  if (frames_.size() < capacity_) {
    at = frames_.size();
    frames_.emplace_back();
  } else {
    at = Victim();
    Frame& old = frames_[at];
    if (old.dirty) pager_.Write(old.id, old.page);
    table_.erase(old.id);
  }
  Frame& frame = frames_[at];
  frame.id = id;
  frame.pins = 1;
  frame.dirty = false;
  frame.referenced = true;
  if (id < pager_.page_count()) {
    pager_.Read(id, frame.page);
  } else {
    frame.page.fill(std::byte{0});
  }
  table_[id] = at;
  return PageHandle(*this, at);
}

size_t BufferPool::Victim() {
  // The first time round, the hand clears the referenced pages it passes, so
  // the second time round it finds one, unless all of them are pinned.
  for (size_t step = 0; step < 2 * frames_.size(); ++step) {
    size_t at = hand_;
    hand_ = (hand_ + 1) % frames_.size();
    Frame& frame = frames_[at];
    if (frame.pins > 0) continue;
    if (frame.referenced) {
      frame.referenced = false;
      continue;
    }
    return at;
  }
  std::fprintf(stderr,
               "smollergraph: the buffer pool's pages are all pinned\n");
  std::abort();
}

void BufferPool::Unpin(size_t frame) { --frames_[frame].pins; }

void BufferPool::Flush() {
  for (Frame& frame : frames_) {
    if (!frame.dirty) continue;
    pager_.Write(frame.id, frame.page);
    frame.dirty = false;
  }
}

void BufferPool::Clear() {
  Flush();
  frames_.clear();
  table_.clear();
  hand_ = 0;
}

}  // namespace smollergraph
