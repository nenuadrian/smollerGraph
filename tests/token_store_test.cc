#include "token_store.h"

#include <cstdio>
#include <optional>
#include <string>

#include "absl/status/status.h"
#include "gtest/gtest.h"

namespace smollergraph {
namespace {

TEST(TokenStoreTest, GivesEachNameOneToken) {
  TokenStore labels;
  ASSERT_EQ(labels.OpenTemporary(), absl::OkStatus());
  EXPECT_EQ(labels.Add("Species"), 0);
  EXPECT_EQ(labels.Add("Genus"), 1);
  EXPECT_EQ(labels.Add("Species"), 0);
  EXPECT_EQ(labels.Find("Genus"), 1);
  EXPECT_EQ(labels.Find("Family"), std::nullopt);
  EXPECT_EQ(labels.Name(1), "Genus");
  EXPECT_EQ(labels.Name(kNoToken), "");
}

TEST(TokenStoreTest, KeepsItsNamesInAFile) {
  std::string path = ::testing::TempDir() + "token_store_test";
  std::remove(path.c_str());
  {
    TokenStore types;
    ASSERT_EQ(types.Open(path), absl::OkStatus());
    types.Add("EATS");
    types.Add("LIVES_IN");
  }
  TokenStore types;
  ASSERT_EQ(types.Open(path), absl::OkStatus());
  EXPECT_EQ(types.Find("LIVES_IN"), 1);
  EXPECT_EQ(types.Name(0), "EATS");
}

}  // namespace
}  // namespace smollergraph
