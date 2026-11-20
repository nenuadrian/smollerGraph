#ifndef SMOLLERGRAPH_EXPORT_H_
#define SMOLLERGRAPH_EXPORT_H_

#include <string>

#include "absl/status/status.h"
#include "smollergraph/graph.h"

namespace smollergraph {

// Writes the graph into the folder at `path`, for programs outside the
// database to read, such as a script that trains a model: nodes.jsonl, a line
// of JSON for each node, with its id, name, label and properties, and
// relationships.csv, a line for each relationship, with the ids of the nodes
// it joins and its type.
absl::Status Export(const Graph& graph, const std::string& path);

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_EXPORT_H_
