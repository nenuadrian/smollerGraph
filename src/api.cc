#include "api.h"

#include <string>

#include "absl/strings/str_cat.h"

namespace smollergraph {

std::string JsonString(std::string_view text) {
  std::string json = "\"";
  for (char c : text) {
    if (c == '"' || c == '\\') {
      json += '\\';
      json += c;
    } else if (static_cast<unsigned char>(c) < 0x20) {
      // A control character, such as a new line, goes in as \u and its code
      // in four hex digits.
      absl::StrAppend(&json, "\\u", absl::Hex(c, absl::kZeroPad4));
    } else {
      json += c;
    }
  }
  return json + '"';
}

std::string GraphJson(const Graph& graph) {
  std::string json = "{\"nodes\": [";
  for (NodeId id = 0; id < graph.node_count(); ++id) {
    if (id > 0) json += ", ";
    absl::StrAppend(&json, "{\"id\": ", id,
                    ", \"name\": ", JsonString(graph.name(id)), "}");
  }
  json += "], \"relationships\": [";
  for (RelationshipId id = 0; id < graph.relationship_count(); ++id) {
    RelationshipRecord record = graph.relationship(id);
    if (id > 0) json += ", ";
    absl::StrAppend(&json, "{\"id\": ", id, ", \"from\": ", record.from,
                    ", \"to\": ", record.to,
                    ", \"type\": ", JsonString(record.type), "}");
  }
  return json + "]}";
}

}  // namespace smollergraph
