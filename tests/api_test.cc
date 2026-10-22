#include "api.h"

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

}  // namespace
}  // namespace smollergraph
