#include "name_index.h"

#include <cstdio>
#include <optional>
#include <string>

#include "absl/status/status.h"
#include "gtest/gtest.h"

namespace smollergraph {
namespace {

TEST(NameIndexTest, FindsWhatItKeeps) {
  NameIndex index;
  ASSERT_EQ(index.OpenTemporary(), absl::OkStatus());
  index.Insert("Red fox", 0);
  index.Insert("Rabbit", 2);
  index.Insert("Clover", 8);
  EXPECT_EQ(index.Find("Rabbit"), 2);
  EXPECT_EQ(index.Find("Red fox"), 0);
  EXPECT_EQ(index.Find("Clover"), 8);
  EXPECT_EQ(index.Find("Badger"), std::nullopt);
}

TEST(NameIndexTest, KeepsItsNamesInAFile) {
  std::string path = ::testing::TempDir() + "name_index_test";
  std::remove(path.c_str());
  {
    NameIndex index;
    ASSERT_EQ(index.Open(path), absl::OkStatus());
    index.Insert("Tawny owl", 4);
  }
  NameIndex index;
  ASSERT_EQ(index.Open(path), absl::OkStatus());
  EXPECT_EQ(index.Find("Tawny owl"), 4);
}

}  // namespace
}  // namespace smollergraph
