#include "facts.h"

#include <string_view>

namespace smollergraph {

bool LoadFacts(std::istream& in, Graph& graph, std::string& error) {
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line.starts_with('#')) continue;

    size_t open = line.find(")-[:");
    size_t close = line.find("]->(");
    if (!line.starts_with('(') || !line.ends_with(')') ||
        open == std::string::npos || close == std::string::npos ||
        close < open) {
      error = line;
      return false;
    }
    std::string_view text = line;
    NodeId from = graph.AddNode(text.substr(1, open - 1));
    std::string_view type = text.substr(open + 4, close - open - 4);
    // What's left after "]->(", without the closing bracket.
    NodeId to = graph.AddNode(text.substr(close + 4, text.size() - close - 5));
    graph.AddRelationship(from, type, to);
  }
  return true;
}

}  // namespace smollergraph
