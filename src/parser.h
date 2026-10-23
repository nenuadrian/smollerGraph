#ifndef SMOLLERGRAPH_PARSER_H_
#define SMOLLERGRAPH_PARSER_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "graph.h"

namespace smollergraph {

// A node in a pattern, such as (a {name: 'Red fox'}). Its variable and its
// name are both optional: () matches any node, and gives it no variable.
struct NodePattern {
  std::string variable;
  std::optional<std::string> name;
};

// A relationship from a node in a pattern to the next one, such as -[:EATS]->,
// or -[:EATS*2]->, which is two of them in a row.
struct RelationshipPattern {
  std::string type;
  int hops = 1;
};

// A query in our subset of Cypher, such as
//   MATCH (a {name: 'Red fox'})-[:EATS*2]->(c) RETURN DISTINCT c.name
// with EXPLAIN in front of it to ask for its plan. Relationship i goes from
// node i to node i + 1.
struct Query {
  bool explain = false;
  std::vector<NodePattern> nodes;
  std::vector<RelationshipPattern> relationships;
  bool distinct = false;
  // The variables whose names are returned, in order.
  std::vector<std::string> returns;
};

// Reads a query. Returns an error that says what's wrong and where, when it
// isn't one our subset has.
absl::StatusOr<Query> Parse(std::string_view text);

// Reads properties as Cypher writes them, in braces, such as
// {legs: 4, diet: 'omnivore'}, with a string or a whole number for each key.
absl::StatusOr<std::vector<Property>> ParseProperties(std::string_view text);

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_PARSER_H_
