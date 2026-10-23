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

// Returns a few of the graph's names that start with `prefix`, in order, for
// the UI to suggest.
std::string NamesJson(const Graph& graph, std::string_view prefix);

// Returns node `id`'s record as it's kept on disk: its label, its page, its
// offset in the page and its bytes, then its chain of relationships, newest
// first, each with its properties, the node it goes to and where it's kept,
// and its chain of properties, newest first, each with where it's kept. An id
// the graph doesn't have is an error.
absl::StatusOr<std::string> NodeJson(const Graph& graph, std::string_view id);

// Parses, plans and runs a query in our subset of Cypher, from a request. It
// answers with the columns and up to kMaxRows of the rows, each value with the
// id of its node, and the nodes the rows return, up to kMaxDrawn of them, with
// the relationships between them, for the page to draw. It says how long the
// query took on the server, and how many pages it read. After EXPLAIN, it
// answers with the plan, and it says what's wrong with a query it can't run.
inline constexpr size_t kMaxRows = 1000;
inline constexpr size_t kMaxDrawn = 500;
absl::StatusOr<std::string> QueryJson(const Graph& graph,
                                      std::string_view text);

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_API_H_
