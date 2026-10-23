#include "api.h"

#include <cstdint>
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

TEST(NamesJsonTest, SuggestsNamesThatStartWithWhatsTyped) {
  Graph graph;
  graph.AddNode("Red fox");
  graph.AddNode("Rabbit");
  graph.AddNode("Clover");
  EXPECT_EQ(NamesJson(graph, "R"), R"({"names": ["Rabbit", "Red fox"]})");
  EXPECT_EQ(NamesJson(graph, "Bad"), R"({"names": []})");
}

TEST(NodeJsonTest, ShowsWhereARecordAndItsChainsAreKept) {
  Graph graph;
  NodeId fox = graph.AddNode("Red fox");
  RelationshipId eats =
      graph.AddRelationship(fox, "EATS", graph.AddNode("Rabbit"));
  graph.SetRelationshipProperty(eats, "source", std::string("Wikipedia"));
  graph.AddRelationship(fox, "LIVES_IN", graph.AddNode("Forest"));
  graph.SetProperty(fox, "legs", int64_t{4});
  graph.SetLabel(fox, "Animal");
  absl::StatusOr<std::string> json = NodeJson(graph, "0");
  ASSERT_TRUE(json.ok()) << json.status();
  // The name and the label, each padded with zeros, then the first
  // relationship's id, 1, with its lowest byte first, as x86 and ARM keep
  // numbers, and the first property's, 1, as the relationship's source is
  // property 0.
  EXPECT_EQ(*json,
            R"({"id": 0, "name": "Red fox", "label": "Animal", "page": 1, )"
            R"("offset": 0, )"
            R"("bytes": "52 65 64 20 66 6f 78 00 00 00 00 00 00 00 00 00)"
            R"(\u000a00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00)"
            R"(\u000a00 00 00 00 00 00 00 00 00 00 00 00 41 6e 69 6d)"
            R"(\u000a61 6c 00 00 00 00 00 00 01 00 00 00 01 00 00 00", )"
            R"("chain": [{"id": 1, "type": "LIVES_IN", "properties": {}, )"
            R"("to": 2, "to_name": "Forest", "to_label": "", "page": 1, )"
            R"("offset": 32}, )"
            R"({"id": 0, "type": "EATS", "properties": )"
            R"({"source": "Wikipedia"}, "to": 1, "to_name": "Rabbit", )"
            R"("to_label": "", "page": 1, "offset": 0}], )"
            R"("properties": [{"id": 1, "key": "legs", "value": 4, )"
            R"("page": 1, "offset": 64}]})");
}

TEST(NodeJsonTest, RefusesANodeTheGraphDoesNotHave) {
  Graph graph;
  graph.AddNode("Red fox");
  EXPECT_TRUE(absl::IsNotFound(NodeJson(graph, "1").status()));
  EXPECT_TRUE(absl::IsNotFound(NodeJson(graph, "fox").status()));
}

// A query's JSON without the time it took, which changes from run to run.
std::string WithoutTime(const std::string& json) {
  return json.substr(json.find("\"pages\""));
}

TEST(QueryJsonTest, AnswersWithRowsOrAPlan) {
  Graph graph;
  NodeId fox = graph.AddNode("Red fox");
  NodeId rabbit = graph.AddNode("Rabbit");
  graph.AddRelationship(fox, "EATS", rabbit);
  graph.AddRelationship(fox, "LIVES_IN", graph.AddNode("Forest"));
  graph.SetProperty(rabbit, "legs", int64_t{4});
  graph.SetLabel(rabbit, "Animal");
  absl::StatusOr<std::string> rows = QueryJson(
      graph,
      "MATCH (a {name: 'Red fox'})-[:EATS]->(b) RETURN b.name, b.legs, a.legs");
  ASSERT_TRUE(rows.ok()) << rows.status();
  // The fox has no legs property here, so a.legs is null. The page draws the
  // fox and the rabbit, and the relationship between them, but not the forest,
  // which no row returns.
  EXPECT_EQ(WithoutTime(*rows),
            R"("pages": 0, "count": 1, )"
            R"("columns": ["b.name", "b.legs", "a.legs"], "rows": )"
            R"([[{"id": 1, "value": "Rabbit"}, {"id": 1, "value": 4}, )"
            R"({"id": 0, "value": null}]], )"
            R"("nodes": [{"id": 1, "name": "Rabbit", "label": "Animal"}, )"
            R"({"id": 0, "name": "Red fox", "label": ""}], )"
            R"("relationships": [{"id": 0, "from": 0, "to": 1, )"
            R"("type": "EATS"}]})");
  absl::StatusOr<std::string> plan =
      QueryJson(graph, "EXPLAIN MATCH (a) RETURN a.name");
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(*plan, R"({"plan": "AllNodesScan (a)\u000aReturn a.name\u000a"})");
  EXPECT_TRUE(absl::IsInvalidArgument(
      QueryJson(graph, "MATCH (a RETURN a.name").status()));
}

}  // namespace
}  // namespace smollergraph
