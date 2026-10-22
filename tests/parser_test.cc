#include "parser.h"

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "gtest/gtest.h"

namespace smollergraph {
namespace {

TEST(ParserTest, ReadsAPattern) {
  absl::StatusOr<Query> query = Parse(
      "MATCH (a {name: 'Red fox'})-[:EATS]->(b)-[:EATS*2]->(c) "
      "RETURN DISTINCT b.name, c.name");
  ASSERT_TRUE(query.ok()) << query.status();
  ASSERT_EQ(query->nodes.size(), 3);
  EXPECT_EQ(query->nodes[0].variable, "a");
  EXPECT_EQ(query->nodes[0].name, "Red fox");
  EXPECT_EQ(query->nodes[2].name, std::nullopt);
  ASSERT_EQ(query->relationships.size(), 2);
  EXPECT_EQ(query->relationships[0].type, "EATS");
  EXPECT_EQ(query->relationships[1].hops, 2);
  EXPECT_TRUE(query->distinct);
  EXPECT_EQ(query->returns, (std::vector<std::string>{"b", "c"}));
}

TEST(ParserTest, ReadsKeywordsInAnyCase) {
  absl::StatusOr<Query> query =
      Parse("explain match (a)-[:IS_A]->() return a.name");
  ASSERT_TRUE(query.ok()) << query.status();
  EXPECT_TRUE(query->explain);
  EXPECT_EQ(query->nodes[1].variable, "");
}

TEST(ParserTest, SaysWhatItExpected) {
  EXPECT_EQ(Parse("MATCH (a RETURN a.name").status().message(),
            "expected ) at 9, not RETURN");
  EXPECT_EQ(Parse("MATCH (a)-[:EATS]-(b) RETURN b.name").status().message(),
            "expected > at 18, not (");
  EXPECT_EQ(Parse("MATCH (a)-[:EATS*20]->(b) RETURN b.name").status().message(),
            "expected a number of hops from 1 to 10 at 17, not 20");
}

}  // namespace
}  // namespace smollergraph
