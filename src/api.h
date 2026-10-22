#ifndef SMOLLERGRAPH_API_H_
#define SMOLLERGRAPH_API_H_

#include <string>
#include <string_view>

#include "graph.h"

namespace smollergraph {

// What sg serve answers with, as JSON. It's kept apart from HTTP, so the tests
// can check it without a server.

// Returns `text` as a JSON string: in quotes, with quotes, backslashes and
// control characters escaped.
std::string JsonString(std::string_view text);

// Returns an error's message as JSON, for the UI to show.
std::string ErrorJson(std::string_view message);

// Returns each node and each relationship in the graph, for the UI to draw.
std::string GraphJson(const Graph& graph);

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_API_H_
