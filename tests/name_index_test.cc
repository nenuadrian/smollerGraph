#include "name_index.h"

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

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

TEST(NameIndexTest, SplitsAsItGrows) {
  NameIndex index;
  ASSERT_EQ(index.OpenTemporary(), absl::OkStatus());
  // Enough names to split pages above the leaves too, in a jumbled order.
  constexpr uint32_t kNames = 20000;
  for (uint32_t i = 0; i < kNames; ++i) {
    uint32_t n = i * 7919 % kNames;
    index.Insert("Person " + std::to_string(n), n);
  }
  for (uint32_t n = 0; n < kNames; ++n) {
    EXPECT_EQ(index.Find("Person " + std::to_string(n)), n);
  }
  EXPECT_EQ(index.Find("Person " + std::to_string(kNames)), std::nullopt);
}

TEST(NameIndexTest, FindsNamesByHowTheyStart) {
  NameIndex index;
  ASSERT_EQ(index.OpenTemporary(), absl::OkStatus());
  for (uint32_t n = 0; n < 1000; ++n) {
    index.Insert("Person " + std::to_string(n), n);
  }
  index.Insert("Rabbit", 1000);
  // Person 12 and the ten names after it, Person 120 to 129, in name order.
  std::vector<std::string> names = index.NamesStartingWith("Person 12", 20);
  ASSERT_EQ(names.size(), 11);
  EXPECT_EQ(names.front(), "Person 12");
  EXPECT_EQ(names.back(), "Person 129");
  EXPECT_EQ(index.NamesStartingWith("Ra", 5),
            std::vector<std::string>{"Rabbit"});
  // The thousand people fill several leaves, and the scan goes through them.
  EXPECT_EQ(index.NamesStartingWith("Person", 2000).size(), 1000);
}

}  // namespace
}  // namespace smollergraph
