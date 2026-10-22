#include "api.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "query.h"

namespace smollergraph {
namespace {

// The most hops a walk can take: the UI draws a few hops, not the whole graph.
constexpr uint32_t kMaxHops = 10;

// Reads `text` as a whole number, if it's one, and it's at most `max`.
std::optional<uint32_t> ParseNumber(std::string_view text, uint32_t max) {
  uint32_t number = 0;
  if (!absl::SimpleAtoi(text, &number) || number > max) return std::nullopt;
  return number;
}

}  // namespace

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

std::string ErrorJson(std::string_view message) {
  return absl::StrCat("{\"error\": ", JsonString(message), "}");
}

absl::StatusOr<std::string> WalkJson(const Graph& graph, std::string_view from,
                                     std::string_view type,
                                     std::string_view hops) {
  std::optional<NodeId> start = graph.Find(from);
  if (!start) {
    return absl::NotFoundError(absl::StrCat("there's no node called ", from));
  }
  std::optional<uint32_t> count = ParseNumber(hops, kMaxHops);
  if (!count) {
    return absl::InvalidArgumentError(
        absl::StrCat("hops is a number from 0 to ", kMaxHops));
  }
  // Each hop's nodes are an array of their ids, and the hops are an array of
  // those.
  std::vector<std::string> arrays;
  for (const std::vector<NodeId>& reached :
       WalkHops(graph, *start, type, *count)) {
    arrays.push_back(absl::StrCat("[", absl::StrJoin(reached, ", "), "]"));
  }
  return absl::StrCat("{\"hops\": [", absl::StrJoin(arrays, ", "), "]}");
}

}  // namespace smollergraph
