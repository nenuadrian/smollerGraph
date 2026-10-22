#include "buffer_pool.h"

#include <atomic>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "pager.h"

namespace smollergraph {
namespace {

TEST(BufferPoolTest, ReadsAPageOnce) {
  Pager pager;
  ASSERT_EQ(pager.OpenTemporary(), absl::OkStatus());
  pager.Write(0, Page{});
  BufferPool pool(pager, 4);
  // Each handle unpins the page at the end of its line.
  pool.Fetch(0);
  pool.Fetch(0);
  EXPECT_EQ(pager.reads(), 1);
}

TEST(BufferPoolTest, WritesBackAChangedPageItEvicts) {
  Pager pager;
  ASSERT_EQ(pager.OpenTemporary(), absl::OkStatus());
  // Two frames, so the third page evicts the first.
  BufferPool pool(pager, 2);
  for (PageId id = 0; id < 3; ++id) {
    PageHandle handle = pool.Fetch(id);
    handle.write()[0] = std::byte(id + 1);
  }
  EXPECT_EQ(pager.writes(), 1);
  PageHandle first = pool.Fetch(0);
  EXPECT_EQ(first.read()[0], std::byte{1});
}

TEST(BufferPoolTest, KeepsAPinnedPage) {
  Pager pager;
  ASSERT_EQ(pager.OpenTemporary(), absl::OkStatus());
  BufferPool pool(pager, 2);
  PageHandle pinned = pool.Fetch(0);
  pinned.write()[0] = std::byte{7};
  for (PageId id = 1; id < 5; ++id) pool.Fetch(id);
  EXPECT_EQ(pinned.read()[0], std::byte{7});
  EXPECT_EQ(pager.writes(), 0);
}

TEST(BufferPoolTest, FlushWritesChangedPages) {
  Pager pager;
  ASSERT_EQ(pager.OpenTemporary(), absl::OkStatus());
  BufferPool pool(pager, 4);
  pool.Fetch(0).write()[5] = std::byte{9};
  pool.Flush();
  Page page;
  pager.Read(0, page);
  EXPECT_EQ(page[5], std::byte{9});
}

TEST(BufferPoolTest, ThreadsFetchAtOnce) {
  Pager pager;
  ASSERT_EQ(pager.OpenTemporary(), absl::OkStatus());
  for (PageId id = 0; id < 64; ++id) {
    Page page{};
    page[0] = std::byte(id);
    pager.Write(id, page);
  }
  // Four frames for 64 pages, so the threads evict each other's pages.
  BufferPool pool(pager, 4);
  std::atomic<int> wrong = 0;
  std::vector<std::thread> threads;
  for (int t = 0; t < 4; ++t) {
    threads.emplace_back([&pool, &wrong, t] {
      for (int i = 0; i < 10000; ++i) {
        PageId id = (i * 7 + t) % 64;
        if (pool.Fetch(id).read()[0] != std::byte(id)) ++wrong;
      }
    });
  }
  for (std::thread& thread : threads) thread.join();
  EXPECT_EQ(wrong, 0);
}

}  // namespace
}  // namespace smollergraph
