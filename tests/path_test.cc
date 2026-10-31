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
  // No EATS reaches the meadow, so the search goes through all six nodes
  // that EATS joins to the fox before it gives up.
  search = ShortestPath(graph_, *graph_.Find("Red fox"), *graph_.Find("Meadow"),
                        "EATS", Direction::kBoth);
  EXPECT_FALSE(search.path);
  EXPECT_EQ(search.reached, 6);
}

}  // namespace
}  // namespace smollergraph
