#ifndef SMOLLERGRAPH_FACTS_H_
#define SMOLLERGRAPH_FACTS_H_

#include <istream>
#include <string>

#include "graph.h"

namespace smollergraph {

// Reads facts into `graph`, one a line, written as Cypher patterns such as
// (Red fox)-[:EATS]->(Rabbit). Blank lines and lines starting with # are
// skipped. On a line it can't read, it stops, puts the line in `error` and
// returns false.
bool LoadFacts(std::istream& in, Graph& graph, std::string& error);

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_FACTS_H_
