// smollerGraph in a browser: the graph runs in the page, as WebAssembly, and
// answers the explorer's questions as sg serve answers them over HTTP. The
// page's files live in Emscripten's file system, in memory, so the graph is
// built from the facts each time the page opens, and nothing is kept.

#include <emscripten/bind.h>

#include <chrono>
#include <fstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "smollergraph/api.h"
#include "smollergraph/embed.h"
#include "smollergraph/facts.h"
#include "smollergraph/graph.h"

namespace smollergraph {
namespace {

// The page's one graph. It's never destroyed, as the page just closes.
Graph& TheGraph() {
  static Graph* graph = new Graph;
  return *graph;
}

// An answer as sg serve sends it: the JSON, or the error's message as JSON.
std::string Answer(const absl::StatusOr<std::string>& json) {
  return json.ok() ? *json : ErrorJson(json.status().message());
}

// Builds the graph in `folder` from the facts at `facts`, gives each node an
// embedding along relationships of `type`, and says how big it is and how
// long that took, in milliseconds.
std::string Build(const std::string& folder, const std::string& facts,
                  const std::string& type) {
  auto start = std::chrono::steady_clock::now();
  Graph& graph = TheGraph();
  if (absl::Status status = graph.Open(folder); !status.ok()) {
    return ErrorJson(status.message());
  }
  std::ifstream file(facts);
  if (absl::Status status = LoadFacts(file, graph); !status.ok()) {
    return ErrorJson(status.message());
  }
  std::vector<Embedding> embeddings = FastRP(graph, type, 42);
  for (NodeId node = 0; node < embeddings.size(); ++node) {
    graph.SetEmbedding(node, embeddings[node]);
  }
  graph.Flush();
  std::chrono::duration<double, std::milli> took =
      std::chrono::steady_clock::now() - start;
  return absl::StrCat("{\"nodes\":", graph.node_count(),
                      ",\"relationships\":", graph.relationship_count(),
                      ",\"ms\":", static_cast<int>(took.count()), "}");
}

std::string Names(const std::string& prefix) {
  return NamesJson(TheGraph(), prefix);
}

std::string Query(const std::string& text) {
  return Answer(QueryJson(TheGraph(), text));
}

std::string Node(const std::string& id) {
  return Answer(NodeJson(TheGraph(), id));
}

std::string Similar(const std::string& id) {
  return Answer(SimilarJson(TheGraph(), id));
}

}  // namespace
}  // namespace smollergraph

EMSCRIPTEN_BINDINGS(smollergraph) {
  emscripten::function("build", &smollergraph::Build);
  emscripten::function("names", &smollergraph::Names);
  emscripten::function("query", &smollergraph::Query);
  emscripten::function("node", &smollergraph::Node);
  emscripten::function("similar", &smollergraph::Similar);
}
