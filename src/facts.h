#ifndef SMOLLERGRAPH_FACTS_H_
#define SMOLLERGRAPH_FACTS_H_

#include <istream>
#include <string>

#include "absl/status/status.h"
#include "graph.h"

namespace smollergraph {

// Reads facts into `graph`, one a line, written as Cypher patterns such as
// (Red fox)-[:EATS]->(Rabbit), or a node on its own, such as (Rabbit). A node
// can have a label after a colon, and nodes and relationships can have
// properties in braces, as in (Rabbit:Animal {legs: 4}). Blank lines and lines
// starting with # are skipped. On a
// line it can't read, it stops, and returns an error with the line as its
// message.
absl::Status LoadFacts(std::istream& in, Graph& graph);

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_FACTS_H_
