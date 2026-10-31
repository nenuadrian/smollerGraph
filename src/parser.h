#ifndef SMOLLERGRAPH_PARSER_H_
#define SMOLLERGRAPH_PARSER_H_

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "graph.h"

namespace smollergraph {

// A node in a pattern, such as (a:Animal {name: 'Red fox'}). Its variable,
// its label and its properties are all optional: () matches any node, and
// gives it no variable.
struct NodePattern {
  std::string variable;
  std::string label;
  std::vector<Property> properties;
};

// The hops of a relationship written with a star and no number, such as
// -[:KNOWS*]-, which stands for any number of them.
inline constexpr int kAnyHops = 0;

// A relationship from a node in a pattern to the next one, such as -[:EATS]->,
// or -[:EATS*2]->, which is two of them in a row. One written <-[:EATS]- goes
// the other way, from the next node to this one, and one written -[:EATS]-,
// with no arrow, goes either way.
struct RelationshipPattern {
  std::string type;
  int hops = 1;
  // Which way to follow it from the node before it: out, in when it's written
  // with its arrow on the left, or both with no arrow.
  Direction direction = Direction::kOut;
};

// A variable's property, such as a.name or b.weight_g.
struct PropertyLookup {
  std::string variable;
  std::string key;
};

// A comparison in WHERE, such as b.weight_g < 1000: a property, an operator,
// which is one of = <> < <= > >=, and a value.
struct Comparison {
  PropertyLookup property;
  std::string op;
  Value value;
};

// What a query returns in a column: a variable's property, such as b.name,
// or with no key, a path's variable, such as p, which returns the path, or
// length(p), which returns how many relationships it has.
struct ReturnItem {
  std::string variable;
  std::string key;
  bool length = false;
};

// A query in our subset of Cypher, such as
//   MATCH (a {name: 'Red fox'})-[:EATS]->(b) WHERE b.weight_g < 1000
//   RETURN DISTINCT b.name
// with EXPLAIN in front of it to ask for its plan. Relationship i joins node i
// to node i + 1.
struct Query {
  bool explain = false;
  // The path's variable, for a pattern written as p = shortestPath(...),
  // which asks for a shortest path between its two nodes.
  std::string path;
  std::vector<NodePattern> nodes;
  std::vector<RelationshipPattern> relationships;
  // The comparisons in WHERE, joined by AND, so a row has to pass them all.
  std::vector<Comparison> where;
  bool distinct = false;
  // What it returns, in order.
  std::vector<ReturnItem> returns;
};

// Reads a query. Returns an error that says what's wrong and where, when it
// isn't one our subset has.
absl::StatusOr<Query> Parse(std::string_view text);

// Reads properties as Cypher writes them, in braces, such as
// {legs: 4, diet: 'omnivore'}, with a string or a whole number for each key.
absl::StatusOr<std::vector<Property>> ParseProperties(std::string_view text);

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_PARSER_H_
