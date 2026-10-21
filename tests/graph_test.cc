#include "graph.h"

#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

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

TEST(GraphTest, KeepsItsRecordsInAFolder) {
  std::string path = ::testing::TempDir() + "graph_test";
  std::filesystem::remove_all(path);
  std::string error;
  {
    Graph graph;
    ASSERT_TRUE(graph.Open(path, error)) << error;
    NodeId rabbit = graph.AddNode("Rabbit");
    graph.AddRelationship(rabbit, "EATS", graph.AddNode("Clover"));
  }
  Graph graph;
  ASSERT_TRUE(graph.Open(path, error)) << error;
  std::optional<NodeId> rabbit = graph.Find("Rabbit");
  ASSERT_TRUE(rabbit);
  EXPECT_EQ(Names(graph, graph.Neighbours(*rabbit, "EATS")),
            std::vector<std::string>{"Clover"});
}

TEST(FactsTest, WalksTwoHopsDownTheFoodChain) {
  std::istringstream facts(
      "# What eats what\n"
      "(Red fox)-[:EATS]->(Rabbit)\n"
      "(Red fox)-[:EATS]->(Field mouse)\n"
      "(Rabbit)-[:EATS]->(Clover)\n"
      "(Field mouse)-[:EATS]->(Wheat)\n");
  Graph graph;
  std::string error;
  ASSERT_TRUE(LoadFacts(facts, graph, error)) << error;
  EXPECT_EQ(Names(graph, Walk(graph, *graph.Find("Red fox"), "EATS", 2)),
            (std::vector<std::string>{"Clover", "Wheat"}));
}

TEST(FactsTest, StopsAtALineThatIsNotAFact) {
  std::istringstream facts("(Red fox) EATS (Rabbit)\n");
  Graph graph;
  std::string error;
  EXPECT_FALSE(LoadFacts(facts, graph, error));
  EXPECT_EQ(error, "(Red fox) EATS (Rabbit)");
}

}  // namespace
}  // namespace smollergraph
