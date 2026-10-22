#ifndef SMOLLERGRAPH_SERVER_H_
#define SMOLLERGRAPH_SERVER_H_

#include <string>

#include "absl/status/status.h"
#include "graph.h"

namespace smollergraph {

// Answers HTTP requests for the graph on `address` and `port`, until the
// process is stopped. Returns an error when it can't listen there.
absl::Status Serve(const Graph& graph, const std::string& address, int port);

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_SERVER_H_
