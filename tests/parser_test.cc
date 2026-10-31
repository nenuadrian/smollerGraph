#include "parser.h"

#include <cstdint>
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
  ASSERT_EQ(query->nodes[0].properties.size(), 1);
  EXPECT_EQ(query->nodes[0].properties[0].value, Value(std::string("Red fox")));
  EXPECT_TRUE(query->nodes[2].properties.empty());
  ASSERT_EQ(query->relationships.size(), 2);
  EXPECT_EQ(query->relationships[0].type, "EATS");
  EXPECT_EQ(query->relationships[1].hops, 2);
  EXPECT_TRUE(query->distinct);
  ASSERT_EQ(query->returns.size(), 2);
  EXPECT_EQ(query->returns[1].variable, "c");
  EXPECT_EQ(query->returns[1].key, "name");
}

TEST(ParserTest, ReadsARelationshipThatPointsLeft) {
  absl::StatusOr<Query> query =
      Parse("MATCH (b {name: 'Field mouse'})<-[:EATS]-(a) RETURN a.name");
  ASSERT_TRUE(query.ok()) << query.status();
  ASSERT_EQ(query->relationships.size(), 1);
  EXPECT_EQ(query->relationships[0].type, "EATS");
  EXPECT_EQ(query->relationships[0].direction, Direction::kIn);
  EXPECT_EQ(query->nodes[1].variable, "a");
}

TEST(ParserTest, ReadsAShortestPath) {
  absl::StatusOr<Query> query = Parse(
      "MATCH p = shortestPath((a {name: 'Tawny owl'})-[:EATS*]-(b {name: "
      "'Rabbit'})) RETURN p, length(p)");
  ASSERT_TRUE(query.ok()) << query.status();
  EXPECT_EQ(query->path, "p");
  ASSERT_EQ(query->relationships.size(), 1);
  EXPECT_EQ(query->relationships[0].hops, kAnyHops);
  EXPECT_EQ(query->relationships[0].direction, Direction::kBoth);
  ASSERT_EQ(query->returns.size(), 2);
  EXPECT_EQ(query->returns[0].variable, "p");
  EXPECT_EQ(query->returns[0].key, "");
  EXPECT_EQ(query->returns[1].function, "length");
}

TEST(ParserTest, ReadsACheapestPath) {
  absl::StatusOr<Query> query = Parse(
      "MATCH p = (a {name: 'Ada'})-[:KNOWS* WSHORTEST(weight)]-(b {name: "
      "'Dee'}) RETURN p, cost(p)");
  ASSERT_TRUE(query.ok()) << query.status();
  EXPECT_EQ(query->path, "p");
  EXPECT_FALSE(query->shortest);
  ASSERT_EQ(query->relationships.size(), 1);
  EXPECT_EQ(query->relationships[0].hops, kAnyHops);
  EXPECT_EQ(query->relationships[0].weight, "weight");
  EXPECT_EQ(query->returns[1].function, "cost");
}

TEST(ParserTest, ReadsWhereAndProperties) {
  absl::StatusOr<Query> query = Parse(
      "MATCH (a:Animal {legs: 2})-[:EATS]->(b) WHERE b.weight_g < 1000 AND "
      "b.diet <> 'herbivore' RETURN a.name, b.weight_g");
  ASSERT_TRUE(query.ok()) << query.status();
  EXPECT_EQ(query->nodes[0].label, "Animal");
  EXPECT_EQ(query->nodes[0].properties[0].value, Value(int64_t{2}));
  ASSERT_EQ(query->where.size(), 2);
  EXPECT_EQ(query->where[0].property.key, "weight_g");
  EXPECT_EQ(query->where[0].op, "<");
  EXPECT_EQ(query->where[1].value, Value(std::string("herbivore")));
  EXPECT_EQ(query->returns[1].key, "weight_g");
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
  EXPECT_EQ(
      Parse("MATCH p = (a)-[:EATS* WSHORTEST]-(b) RETURN p").status().message(),
      "expected ( at 31, not ]");
  EXPECT_EQ(Parse("MATCH (a)<-[:EATS]->(b) RETURN b.name").status().message(),
            "expected ( at 19, not >");
  EXPECT_EQ(Parse("MATCH (a)-[:EATS*20]->(b) RETURN b.name").status().message(),
            "expected a number of hops from 1 to 10 at 17, not 20");
  EXPECT_EQ(
      Parse("MATCH (a) WHERE a.legs is 2 RETURN a.name").status().message(),
      "expected a comparison at 23, not is");
}

}  // namespace
}  // namespace smollergraph
