#include "facts.h"

#include <string>
#include <string_view>

namespace smollergraph {

absl::Status LoadFacts(std::istream& in, Graph& graph) {
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line.starts_with('#')) continue;

    size_t open = line.find(")-[:");
    size_t close = line.find("]->(");
    if (!line.starts_with('(') || !line.ends_with(')') ||
        open == std::string::npos || close == std::string::npos ||
        close < open) {
      return absl::InvalidArgumentError(line);
    }
    std::string_view text = line;
    std::string_view from = text.substr(1, open - 1);
    std::string_view type = text.substr(open + 4, close - open - 4);
    // What's left after "]->(", without the closing bracket.
    std::string_view to = text.substr(close + 4, text.size() - close - 5);
    // Names and types have to fit in the graph's records.
    if (from.size() > Graph::kMaxName || to.size() > Graph::kMaxName ||
        type.size() > Graph::kMaxType) {
      return absl::InvalidArgumentError(line);
    }
    NodeId from_id = graph.AddNode(from);
    NodeId to_id = graph.AddNode(to);
    graph.AddRelationship(from_id, type, to_id);
  }
  return absl::OkStatus();
}

}  // namespace smollergraph
