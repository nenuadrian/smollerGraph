#include "smollergraph/export.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "smollergraph/api.h"

namespace smollergraph {

absl::Status Export(const Graph& graph, const std::string& path) {
  // A folder that's there already, from an earlier run, is fine.
  std::error_code error;
  std::filesystem::create_directory(path, error);
  if (error) {
    return absl::UnavailableError(
        absl::StrCat("can't make ", path, ": ", error.message()));
  }
  std::ofstream nodes(path + "/nodes.jsonl");
  std::ofstream relationships(path + "/relationships.csv");
  if (!nodes || !relationships) {
    return absl::UnavailableError(absl::StrCat("can't write in ", path));
  }
  // JSON Lines: each line is a JSON object of its own, so a reader can take
  // the file a line at a time.
  for (NodeId node = 0; node < graph.node_count(); ++node) {
    nodes << "{\"id\": " << node
          << ", \"name\": " << JsonString(graph.name(node))
          << ", \"label\": " << JsonString(graph.label(node))
          << ", \"properties\": " << PropertiesJson(graph.Properties(node))
          << "}\n";
  }
  relationships << "from,to,type\n";
  for (RelationshipId id = 0; id < graph.relationship_count(); ++id) {
    RelationshipRecord record = graph.relationship(id);
    relationships << record.from << ',' << record.to << ',' << graph.type(id)
                  << '\n';
  }
  return absl::OkStatus();
}

}  // namespace smollergraph
