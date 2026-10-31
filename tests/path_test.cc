#include <sstream>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "facts.h"
#include "graph.h"
#include "gtest/gtest.h"
#include "query.h"

namespace smollergraph {
namespace {

class PathTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::istringstream facts(
        "(Red fox)-[:EATS]->(Rabbit)\n"
        "(Red fox)-[:EATS]->(Field mouse)\n"
        "(Tawny owl)-[:EATS]->(Field mouse)\n"
        "(Rabbit)-[:EATS]->(Clover)\n"
        "(Field mouse)-[:EATS]->(Wheat)\n"
        "(Rabbit)-[:LIVES_IN]->(Meadow)\n");
    ASSERT_EQ(LoadFacts(facts, graph_), absl::OkStatus());
  }

  // The shortest path along EATS, as Cypher writes it, or none.
  std::string Shortest(std::string_view from, std::string_view to,
                       Direction direction) {
    PathSearch search = ShortestPath(graph_, *graph_.Find(from),
                                     *graph_.Find(to), "EATS", direction);
    return search.path ? PathText(graph_, *search.path) : "none";
  }

  Graph graph_;
};

TEST_F(PathTest, FindsAShortestPathEitherWay) {
  // The fox eats the rabbit and the field mouse, which the owl eats too, so
  // the path from the owl to the rabbit goes through the fox, against the
  // arrow of the relationship in the middle.
  EXPECT_EQ(Shortest("Tawny owl", "Rabbit", Direction::kBoth),
            "(Tawny owl)-[:EATS]->(Field mouse)<-[:EATS]-(Red fox)"
            "-[:EATS]->(Rabbit)");
  EXPECT_EQ(Shortest("Tawny owl", "Rabbit", Direction::kOut), "none");
  EXPECT_EQ(Shortest("Red fox", "Clover", Direction::kOut),
            "(Red fox)-[:EATS]->(Rabbit)-[:EATS]->(Clover)");
}

TEST_F(PathTest, SaysHowFarItSearched) {
  PathSearch search =
      ShortestPath(graph_, *graph_.Find("Red fox"), *graph_.Find("Red fox"),
                   "EATS", Direction::kBoth);
  ASSERT_TRUE(search.path);
  EXPECT_EQ(PathText(graph_, *search.path), "(Red fox)");
  // No EATS reaches the meadow, so the side that starts from it runs out after
  // a hop, with the fox's side at its two meals, four nodes in all.
  search = ShortestPath(graph_, *graph_.Find("Red fox"), *graph_.Find("Meadow"),
                        "EATS", Direction::kBoth);
  EXPECT_FALSE(search.path);
  EXPECT_EQ(search.reached, 4);
}

TEST(CheapestPathTest, TakesTheCheaperWayWithMoreHops) {
  // Ada knows Dee, with no weight, and through Bob, at a cost of 31, or
  // through Cy and Eve, at 9, if Cy's relationship with Eve is followed
  // backwards.
  std::istringstream facts(
      "(Ada)-[:KNOWS]->(Dee)\n"
      "(Ada)-[:KNOWS {weight: 30}]->(Bob)\n"
      "(Bob)-[:KNOWS {weight: 1}]->(Dee)\n"
      "(Ada)-[:KNOWS {weight: 2}]->(Cy)\n"
      "(Eve)-[:KNOWS {weight: 3}]->(Cy)\n"
      "(Eve)-[:KNOWS {weight: 4}]->(Dee)\n");
  Graph graph;
  ASSERT_EQ(LoadFacts(facts, graph), absl::OkStatus());
  NodeId ada = *graph.Find("Ada");
  NodeId dee = *graph.Find("Dee");
  PathSearch search =
      CheapestPath(graph, ada, dee, "KNOWS", Direction::kBoth, "weight");
  ASSERT_TRUE(search.path);
  EXPECT_EQ(PathText(graph, *search.path),
            "(Ada)-[:KNOWS {weight: 2}]->(Cy)<-[:KNOWS {weight: 3}]-(Eve)"
            "-[:KNOWS {weight: 4}]->(Dee)");
  EXPECT_EQ(search.path->cost, 9);
  // Following the arrows, the way through Bob is the one left.
  search = CheapestPath(graph, ada, dee, "KNOWS", Direction::kOut, "weight");
  ASSERT_TRUE(search.path);
  EXPECT_EQ(search.path->cost, 31);
  // The shortest path takes the relationship with no weight, which the
  // cheapest path can't add up, so it doesn't follow it.
  search = ShortestPath(graph, ada, dee, "KNOWS", Direction::kBoth);
  ASSERT_TRUE(search.path);
  EXPECT_EQ(PathText(graph, *search.path), "(Ada)-[:KNOWS]->(Dee)");
}

}  // namespace
}  // namespace smollergraph
