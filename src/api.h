#ifndef SMOLLERGRAPH_API_H_
#define SMOLLERGRAPH_API_H_

#include <string>
#include <string_view>

#include "absl/status/statusor.h"
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

// Returns the nodes each hop of a walk reaches, as WalkHops does, for a walk
// whose start, relationship type and hops come from a request. When they don't
// make a walk, it returns an error saying why: not found for a start the graph
// doesn't have, and an invalid argument for hops that aren't a number up to the
// limit.
absl::StatusOr<std::string> WalkJson(const Graph& graph, std::string_view from,
                                     std::string_view type,
                                     std::string_view hops);

// Returns node `id`'s record as it's kept on disk: its page, its offset in the
// page and its bytes, then its chain of relationships, newest first, each with
// where it's kept. An id the graph doesn't have is an error.
absl::StatusOr<std::string> NodeJson(const Graph& graph, std::string_view id);

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_API_H_
