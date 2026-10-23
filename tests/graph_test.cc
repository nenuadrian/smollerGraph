#include "graph.h"

#include <cstdint>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "facts.h"
#include "gtest/gtest.h"
#include "query.h"

namespace smollergraph {
namespace {

std::vector<std::string> Names(const Graph& graph,
                               const std::vector<NodeId>& nodes) {
  std::vector<std::string> names;
  for (NodeId node : nodes) names.push_back(graph.name(node));
  return names;
}

TEST(GraphTest, AddsEachNameOnce) {
  Graph graph;
  NodeId rabbit = graph.AddNode("Rabbit");
  EXPECT_EQ(graph.AddNode("Rabbit"), rabbit);
  EXPECT_EQ(graph.node_count(), 1);
}

TEST(GraphTest, NeighboursFollowOneType) {
  Graph graph;
  NodeId fox = graph.AddNode("Red fox");
  graph.AddRelationship(fox, "EATS", graph.AddNode("Rabbit"));
  graph.AddRelationship(fox, "LIVES_IN", graph.AddNode("Forest"));
  EXPECT_EQ(Names(graph, graph.Neighbours(fox, "EATS")),
            std::vector<std::string>{"Rabbit"});
}

TEST(GraphTest, KeepsPropertiesInAChain) {
  Graph graph;
  NodeId fox = graph.AddNode("Red fox");
  graph.SetProperty(fox, "weight_g", int64_t{5000});
  graph.SetProperty(fox, "diet", std::string("omnivore"));
  // Setting a key again replaces its value, in the record it already has.
  graph.SetProperty(fox, "weight_g", int64_t{6000});
  EXPECT_EQ(graph.FindProperty(fox, "weight_g"), Value(int64_t{6000}));
  EXPECT_EQ(graph.FindProperty(fox, "name"), Value(std::string("Red fox")));
  EXPECT_EQ(graph.FindProperty(fox, "legs"), std::nullopt);
  std::vector<Property> properties = graph.Properties(fox);
  ASSERT_EQ(properties.size(), 2);
  EXPECT_EQ(properties[0].key, "weight_g");
  EXPECT_EQ(properties[1].value, Value(std::string("omnivore")));

  RelationshipId eats =
      graph.AddRelationship(fox, "EATS", graph.AddNode("Rabbit"));
  graph.SetRelationshipProperty(eats, "source", std::string("Wikipedia"));
  ASSERT_EQ(graph.RelationshipProperties(eats).size(), 1);
  EXPECT_EQ(graph.RelationshipProperties(eats)[0].value,
            Value(std::string("Wikipedia")));
}

TEST(GraphTest, KeepsItsRecordsInAFolder) {
  // A new folder, with a name of its own, so nothing is left in it from an
  // earlier run.
  std::string path = ::testing::TempDir() + "graph_test_XXXXXX";
  ASSERT_NE(mkdtemp(path.data()), nullptr);
  {
    Graph graph;
    ASSERT_EQ(graph.Open(path), absl::OkStatus());
    NodeId rabbit = graph.AddNode("Rabbit");
    graph.AddRelationship(rabbit, "EATS", graph.AddNode("Clover"));
    graph.SetProperty(rabbit, "legs", int64_t{4});
  }
  Graph graph;
  ASSERT_EQ(graph.Open(path), absl::OkStatus());
  std::optional<NodeId> rabbit = graph.Find("Rabbit");
  ASSERT_TRUE(rabbit);
  EXPECT_EQ(Names(graph, graph.Neighbours(*rabbit, "EATS")),
            std::vector<std::string>{"Clover"});
  EXPECT_EQ(graph.FindProperty(*rabbit, "legs"), Value(int64_t{4}));
}

TEST(FactsTest, WalksTwoHopsDownTheFoodChain) {
  std::istringstream facts(
      "# What eats what\n"
      "(Red fox)-[:EATS]->(Rabbit)\n"
      "(Red fox)-[:EATS]->(Field mouse)\n"
      "(Rabbit)-[:EATS]->(Clover)\n"
      "(Field mouse)-[:EATS]->(Wheat)\n");
  Graph graph;
  ASSERT_EQ(LoadFacts(facts, graph), absl::OkStatus());
  EXPECT_EQ(Names(graph, Walk(graph, *graph.Find("Red fox"), "EATS", 2)),
            (std::vector<std::string>{"Clover", "Wheat"}));
}

TEST(FactsTest, ReadsPropertiesInBraces) {
  std::istringstream facts(
      "(Red fox {legs: 4, diet: 'omnivore'})\n"
      "(Red fox)-[:EATS {source: 'Wikipedia'}]->(Rabbit {legs: 4})\n");
  Graph graph;
  ASSERT_EQ(LoadFacts(facts, graph), absl::OkStatus());
  NodeId fox = *graph.Find("Red fox");
  EXPECT_EQ(graph.FindProperty(fox, "diet"), Value(std::string("omnivore")));
  EXPECT_EQ(graph.FindProperty(*graph.Find("Rabbit"), "legs"),
            Value(int64_t{4}));
  ASSERT_EQ(graph.RelationshipProperties(0).size(), 1);
  EXPECT_EQ(graph.RelationshipProperties(0)[0].value,
            Value(std::string("Wikipedia")));
}

TEST(FactsTest, StopsAtALineThatIsNotAFact) {
  // Not a pattern, a value that isn't a string or a number, and a name set
  // as a property.
  for (std::string line : {"(Red fox) EATS (Rabbit)", "(Red fox {legs: four})",
                           "(Red fox {name: 'Fox'})"}) {
    std::istringstream facts(line + "\n");
    Graph graph;
    absl::Status status = LoadFacts(facts, graph);
    EXPECT_TRUE(absl::IsInvalidArgument(status));
    EXPECT_EQ(status.message(), line);
  }
}

}  // namespace
}  // namespace smollergraph
