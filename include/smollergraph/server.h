#ifndef SMOLLERGRAPH_SERVER_H_
#define SMOLLERGRAPH_SERVER_H_

#include <string>

#include "absl/status/status.h"
#include "smollergraph/graph.h"

namespace smollergraph {

// Answers HTTP requests for the graph on `address` and `port`, with the UI's
// page from the folder `ui`, until the process is stopped. Returns an error
// when there's no such folder, or it can't listen there.
absl::Status Serve(const Graph& graph, const std::string& ui,
                   const std::string& address, int port);

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_SERVER_H_
