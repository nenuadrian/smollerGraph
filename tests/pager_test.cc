#include "pager.h"

#include <cstdio>
#include <string>

#include "absl/status/status.h"
#include "gtest/gtest.h"

namespace smollergraph {
namespace {

TEST(PagerTest, ReadsBackWhatItWrote) {
  Pager pager;
  ASSERT_EQ(pager.OpenTemporary(), absl::OkStatus());
  Page page{};
  page[0] = std::byte{42};
  page[kPageSize - 1] = std::byte{7};
  pager.Write(0, page);
  pager.Write(1, Page{});
  EXPECT_EQ(pager.page_count(), 2);

  Page read;
  pager.Read(0, read);
  EXPECT_EQ(read, page);
}

TEST(PagerTest, KeepsItsPagesInTheFile) {
  std::string path = ::testing::TempDir() + "pager_test";
  std::remove(path.c_str());
  {
    Pager pager;
    ASSERT_EQ(pager.Open(path), absl::OkStatus());
    Page page{};
    page[100] = std::byte{1};
    pager.Write(0, page);
  }
  Pager pager;
  ASSERT_EQ(pager.Open(path), absl::OkStatus());
  EXPECT_EQ(pager.page_count(), 1);
  Page read;
  pager.Read(0, read);
  EXPECT_EQ(read[100], std::byte{1});
}

}  // namespace
}  // namespace smollergraph
