#include "api.h"

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "graph.h"
#include "gtest/gtest.h"

namespace smollergraph {
namespace {

TEST(JsonStringTest, EscapesWhatJsonDoesNotAllow) {
  EXPECT_EQ(JsonString("Red fox"), R"("Red fox")");
  EXPECT_EQ(JsonString("a \"b\" \\ c\n"), R"("a \"b\" \\ c\u000a")");
}

TEST(GraphJsonTest, ListsNodesAndRelationships) {
  Graph graph;
  NodeId fox = graph.AddNode("Red fox");
  graph.AddRelationship(fox, "EATS", graph.AddNode("Rabbit"));
  EXPECT_EQ(GraphJson(graph),
            R"({"nodes": [{"id": 0, "name": "Red fox"}, )"
            R"({"id": 1, "name": "Rabbit"}], )"
            R"("relationships": [{"id": 0, "from": 0, "to": 1, )"
            R"("type": "EATS"}]})");
}

TEST(WalkJsonTest, ListsTheNodesEachHopReaches) {
  Graph graph;
  NodeId fox = graph.AddNode("Red fox");
  NodeId rabbit = graph.AddNode("Rabbit");
  graph.AddRelationship(fox, "EATS", rabbit);
  graph.AddRelationship(rabbit, "EATS", graph.AddNode("Clover"));
  absl::StatusOr<std::string> json = WalkJson(graph, "Red fox", "EATS", "2");
  ASSERT_TRUE(json.ok()) << json.status();
  EXPECT_EQ(*json, R"({"hops": [[0], [1], [2]]})");
}

TEST(WalkJsonTest, SaysWhatsWrongWithARequest) {
  Graph graph;
  graph.AddNode("Red fox");
  EXPECT_TRUE(
      absl::IsNotFound(WalkJson(graph, "Badger", "EATS", "2").status()));
  EXPECT_TRUE(absl::IsInvalidArgument(
      WalkJson(graph, "Red fox", "EATS", "two").status()));
  EXPECT_TRUE(absl::IsInvalidArgument(
      WalkJson(graph, "Red fox", "EATS", "11").status()));
}

TEST(NodeJsonTest, ShowsWhereARecordAndItsChainAreKept) {
  Graph graph;
  NodeId fox = graph.AddNode("Red fox");
  graph.AddRelationship(fox, "EATS", graph.AddNode("Rabbit"));
  graph.AddRelationship(fox, "LIVES_IN", graph.AddNode("Forest"));
  absl::StatusOr<std::string> json = NodeJson(graph, "0");
  ASSERT_TRUE(json.ok()) << json.status();
  // The name, padded with zeros, then the first relationship's id, 1, with its
  // lowest byte first, as x86 and ARM keep numbers.
  EXPECT_EQ(*json,
            R"({"id": 0, "name": "Red fox", "page": 1, "offset": 0, )"
            R"("bytes": "52 65 64 20 66 6f 78 00 00 00 00 00 00 00 00 00)"
            R"(\u000a00 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00", )"
            R"("chain": [{"id": 1, "type": "LIVES_IN", "to": 2, )"
            R"("to_name": "Forest", "page": 1, "offset": 32}, )"
            R"({"id": 0, "type": "EATS", "to": 1, "to_name": "Rabbit", )"
            R"("page": 1, "offset": 0}]})");
}

TEST(NodeJsonTest, RefusesANodeTheGraphDoesNotHave) {
  Graph graph;
  graph.AddNode("Red fox");
  EXPECT_TRUE(absl::IsNotFound(NodeJson(graph, "1").status()));
  EXPECT_TRUE(absl::IsNotFound(NodeJson(graph, "fox").status()));
}

}  // namespace
}  // namespace smollergraph
