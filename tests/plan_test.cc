#include "plan.h"

#include <string>

#include "absl/status/statusor.h"
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

TEST(PlanTest, ScansWhenTheStartHasNoName) {
  EXPECT_EQ(Explained("MATCH (a)-[:LIVES_IN]->(p {name: 'Meadow'}) "
                      "RETURN a.name"),
            "AllNodesScan (a)\n"
            "Expand (a)-[:LIVES_IN]->(p)\n"
            "Filter (p {name: 'Meadow'})\n"
            "Return a.name\n");
}

TEST(PlanTest, RefusesVariablesItCannotPlan) {
  EXPECT_EQ(Explained("MATCH (a) RETURN b.name"),
            "there's no variable called b");
  EXPECT_EQ(Explained("MATCH (a)-[:EATS]->(a) RETURN a.name"),
            "the variable a is on two nodes");
}

}  // namespace
}  // namespace smollergraph
