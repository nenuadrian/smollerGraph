#include "plan.h"

#include <sstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "facts.h"
#include "gtest/gtest.h"
#include "parser.h"

namespace smollergraph {
namespace {

// The plan for a query, as EXPLAIN shows it, or the error.
std::string Explained(const std::string& text) {
  absl::StatusOr<Query> query = Parse(text);
  if (!query.ok()) return std::string(query.status().message());
  absl::StatusOr<Plan> plan = PlanQuery(*query);
  if (!plan.ok()) return std::string(plan.status().message());
  return Explain(*plan);
}

TEST(PlanTest, StartsFromTheIndexWhenTheStartHasAName) {
  EXPECT_EQ(Explained("MATCH (a {name: 'Red fox'})-[:EATS*2]->(c) "
                      "RETURN DISTINCT c.name"),
            "NodeIndexSeek (a {name: 'Red fox'})\n"
            "Expand (a)-[:EATS]->(_1)\n"
            "Expand (_1)-[:EATS]->(c)\n"
            "Return DISTINCT c.name\n");
}

TEST(PlanTest, ExpandsTheWayTheArrowPoints) {
  EXPECT_EQ(Explained("MATCH (b {name: 'Field mouse'})<-[:EATS]-(a) "
                      "RETURN a.name"),
            "NodeIndexSeek (b {name: 'Field mouse'})\n"
            "Expand (b)<-[:EATS]-(a)\n"
            "Return a.name\n");
}

TEST(PlanTest, StartsFromTheNodeThatIsCheapestToFind) {
  EXPECT_EQ(Explained("MATCH (a)-[:LIVES_IN]->(p {name: 'Meadow'}) "
                      "RETURN a.name"),
            "NodeIndexSeek (p {name: 'Meadow'})\n"
            "Expand (p)<-[:LIVES_IN]-(a)\n"
            "Return a.name\n");
  // The family has a name, so the plan finds it in the index, and walks down
  // to its species, in place of scanning each species.
  EXPECT_EQ(Explained("MATCH (s:Species)-[:PARENT_TAXON*2]->(f:Family "
                      "{name: 'Manidae'}) WHERE s.status = 'endangered' "
                      "RETURN s.name"),
            "NodeIndexSeek (f {name: 'Manidae'})\n"
            "Filter f:Family\n"
            "Expand (f)<-[:PARENT_TAXON]-(_1)\n"
            "Expand (_1)<-[:PARENT_TAXON]-(s)\n"
            "Filter s:Species\n"
            "Filter s.status = 'endangered'\n"
            "Return s.name\n");
}

TEST(PlanTest, FiltersANodeAsSoonAsItHasASlot) {
  EXPECT_EQ(Explained("MATCH (a {legs: 2})-[:EATS]->(b) "
                      "WHERE b.diet = 'omnivore' AND a.weight_g > 100 "
                      "RETURN b.name"),
            "AllNodesScan (a)\n"
            "Filter a.legs = 2\n"
            "Filter a.weight_g > 100\n"
            "Expand (a)-[:EATS]->(b)\n"
            "Filter b.diet = 'omnivore'\n"
            "Return b.name\n");
}

TEST(PlanTest, ScansALabelOrFiltersOnIt) {
  EXPECT_EQ(Explained("MATCH (a:Animal)-[:EATS]->(b:Plant) RETURN a.name"),
            "NodeByLabelScan (a:Animal)\n"
            "Expand (a)-[:EATS]->(b)\n"
            "Filter b:Plant\n"
            "Return a.name\n");
  // The index finds a name more quickly than a scan finds a label.
  EXPECT_EQ(Explained("MATCH (a:Animal {name: 'Red fox'}) RETURN a.name"),
            "NodeIndexSeek (a {name: 'Red fox'})\n"
            "Filter a:Animal\n"
            "Return a.name\n");
}

TEST(PlanTest, FindsBothEndsOfAShortestPath) {
  EXPECT_EQ(Explained("MATCH p = shortestPath((a {name: 'Tawny owl'})"
                      "-[:EATS*]-(b:Animal)) RETURN b.name, length(p)"),
            "NodeIndexSeek (a {name: 'Tawny owl'})\n"
            "NodeByLabelScan (b:Animal)\n"
            "ShortestPath (a)-[:EATS*]-(b)\n"
            "Return b.name, length(p)\n");
  EXPECT_EQ(Explained("MATCH (a)-[:EATS*]->(b) RETURN b.name"),
            "a relationship with any number of hops needs shortestPath");
}

TEST(PlanTest, RefusesVariablesItCannotPlan) {
  EXPECT_EQ(Explained("MATCH (a) RETURN b.name"),
            "there's no variable called b");
  EXPECT_EQ(Explained("MATCH (a) WHERE b.legs = 2 RETURN a.name"),
            "there's no variable called b");
  EXPECT_EQ(Explained("MATCH (a)-[:EATS]->(a) RETURN a.name"),
            "the variable a is on two nodes");
}

// The values a query returns, a row to a string, with commas between columns.
std::vector<std::string> Answers(const Graph& graph, const std::string& text) {
  absl::StatusOr<Query> query = Parse(text);
  if (!query.ok()) return {std::string(query.status().message())};
  absl::StatusOr<Plan> plan = PlanQuery(*query);
  if (!plan.ok()) return {std::string(plan.status().message())};
  std::vector<std::string> answers;
  for (const std::vector<Cell>& row : Run(*plan, graph).rows) {
    std::string answer;
    for (const Cell& cell : row) {
      answer += (answer.empty() ? "" : ", ") +
                (cell.value ? ValueText(*cell.value) : "null");
    }
    answers.push_back(answer);
  }
  return answers;
}

class RunTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::istringstream facts(
        "(Red fox:Animal {weight_g: 6000, diet: 'omnivore'})\n"
        "(Rabbit:Animal {weight_g: 2000, diet: 'herbivore'})\n"
        "(Field mouse:Animal {weight_g: 25, diet: 'omnivore'})\n"
        "(Tawny owl:Animal {weight_g: 500, diet: 'carnivore'})\n"
        "(Clover:Plant)\n"
        "(Wheat:Plant)\n"
        "(Red fox)-[:EATS]->(Rabbit)\n"
        "(Red fox)-[:EATS]->(Field mouse)\n"
        "(Tawny owl)-[:EATS]->(Field mouse)\n"
        "(Rabbit)-[:EATS]->(Clover)\n"
        "(Field mouse)-[:EATS]->(Wheat)\n"
        "(Rabbit)-[:LIVES_IN]->(Meadow)\n"
        "(Field mouse)-[:LIVES_IN]->(Meadow)\n");
    ASSERT_EQ(LoadFacts(facts, graph_), absl::OkStatus());
  }

  Graph graph_;
};

TEST_F(RunTest, WalksDownTheFoodChain) {
  EXPECT_EQ(Answers(graph_,
                    "MATCH (a {name: 'Red fox'})-[:EATS*2]->(c) "
                    "RETURN a.name, c.name"),
            (std::vector<std::string>{"Red fox, Clover", "Red fox, Wheat"}));
}

TEST_F(RunTest, FollowsRelationshipsBackwards) {
  // What eats the field mouse, and what eats what eats the wheat.
  EXPECT_EQ(Answers(graph_,
                    "MATCH (b {name: 'Field mouse'})<-[:EATS]-(a) "
                    "RETURN a.name"),
            (std::vector<std::string>{"Red fox", "Tawny owl"}));
  EXPECT_EQ(Answers(graph_,
                    "MATCH (w {name: 'Wheat'})<-[:EATS]-(m)<-[:EATS]-(a) "
                    "RETURN m.name, a.name"),
            (std::vector<std::string>{"Field mouse, Red fox",
                                      "Field mouse, Tawny owl"}));
}

TEST_F(RunTest, StartsInTheMiddle) {
  // The plan finds the field mouse, goes right to what it eats, then left to
  // what eats it.
  EXPECT_EQ(Answers(graph_,
                    "MATCH (a)-[:EATS]->(m {name: 'Field mouse'})-[:EATS]->(w) "
                    "RETURN a.name, w.name"),
            (std::vector<std::string>{"Red fox, Wheat", "Tawny owl, Wheat"}));
}

TEST_F(RunTest, FollowsARelationshipEitherWay) {
  // What the field mouse eats, then what eats it.
  EXPECT_EQ(Answers(graph_,
                    "MATCH (m {name: 'Field mouse'})-[:EATS]-(b) "
                    "RETURN b.name"),
            (std::vector<std::string>{"Wheat", "Red fox", "Tawny owl"}));
}

TEST_F(RunTest, FindsAShortestPath) {
  EXPECT_EQ(Answers(graph_,
                    "MATCH p = shortestPath((a {name: 'Tawny owl'})-[:EATS*]-"
                    "(b {name: 'Rabbit'})) RETURN p, length(p)"),
            std::vector<std::string>{
                "(Tawny owl)-[:EATS]->(Field mouse)<-[:EATS]-(Red fox)"
                "-[:EATS]->(Rabbit), 3"});
  // Following the arrows, there's no path from the owl to the rabbit, so
  // there's no row.
  EXPECT_TRUE(Answers(graph_,
                      "MATCH p = shortestPath((a {name: 'Tawny owl'})"
                      "-[:EATS*]->(b {name: 'Rabbit'})) RETURN p")
                  .empty());
}

TEST_F(RunTest, FiltersALaterNodeByName) {
  EXPECT_EQ(Answers(graph_,
                    "MATCH (a)-[:LIVES_IN]->(p {name: 'Meadow'}) "
                    "RETURN a.name"),
            (std::vector<std::string>{"Rabbit", "Field mouse"}));
}

TEST_F(RunTest, ReturnsDistinctRows) {
  // The fox and the owl both eat the field mouse, so it's in two rows, and
  // DISTINCT keeps one.
  EXPECT_EQ(Answers(graph_, "MATCH (a)-[:EATS]->(b) RETURN b.name").size(), 5);
  EXPECT_EQ(
      Answers(graph_, "MATCH (a)-[:EATS]->(b) RETURN DISTINCT b.name"),
      (std::vector<std::string>{"Rabbit", "Field mouse", "Clover", "Wheat"}));
  // DISTINCT compares values: the fox and the field mouse are both omnivores.
  EXPECT_EQ(Answers(graph_, "MATCH (a)-[:EATS]->(b) RETURN DISTINCT a.diet"),
            (std::vector<std::string>{"omnivore", "herbivore", "carnivore"}));
}

TEST_F(RunTest, MatchesLabels) {
  EXPECT_EQ(
      Answers(graph_,
              "MATCH (a:Animal)-[:EATS]->(b:Plant) RETURN a.name, b.name"),
      (std::vector<std::string>{"Rabbit, Clover", "Field mouse, Wheat"}));
}

TEST_F(RunTest, FiltersAndReturnsProperties) {
  EXPECT_EQ(Answers(graph_,
                    "MATCH (a)-[:EATS]->(b) WHERE b.weight_g < 1000 "
                    "RETURN a.name, b.name, b.weight_g"),
            (std::vector<std::string>{"Red fox, Field mouse, 25",
                                      "Tawny owl, Field mouse, 25"}));
  // The clover has no weight, so it returns null.
  EXPECT_EQ(Answers(graph_,
                    "MATCH (a {name: 'Rabbit'})-[:EATS]->(b) "
                    "RETURN b.name, b.weight_g"),
            std::vector<std::string>{"Clover, null"});
}

}  // namespace
}  // namespace smollergraph
