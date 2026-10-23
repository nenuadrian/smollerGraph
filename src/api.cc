#include "api.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "parser.h"
#include "plan.h"
#include "record_file.h"

namespace smollergraph {
namespace {

// How many names the UI suggests at a time.
constexpr size_t kSuggestions = 8;

// Reads `text` as a whole number, if it's one, and it's at most `max`.
std::optional<uint32_t> ParseNumber(std::string_view text, uint32_t max) {
  uint32_t number = 0;
  if (!absl::SimpleAtoi(text, &number) || number > max) return std::nullopt;
  return number;
}

// The page a record is kept on, and where it starts in that page, as fields of
// a JSON object.
template <typename Record>
std::string Where(uint32_t id) {
  return absl::StrCat(", \"page\": ", RecordFile<Record>::PageOf(id),
                      ", \"offset\": ", RecordFile<Record>::OffsetOf(id));
}

// A value as JSON: a number as it is, a string in quotes, and null for a
// missing one.
std::string ValueJson(const std::optional<Value>& value) {
  if (!value) return "null";
  if (const int64_t* number = std::get_if<int64_t>(&*value)) {
    return absl::StrCat(*number);
  }
  return JsonString(std::get<std::string>(*value));
}

// Properties as a JSON object, with a field for each key.
std::string PropertiesJson(const std::vector<Property>& properties) {
  std::string json = "{";
  for (size_t i = 0; i < properties.size(); ++i) {
    absl::StrAppend(&json, i > 0 ? ", " : "", JsonString(properties[i].key),
                    ": ", ValueJson(properties[i].value));
  }
  return json + "}";
}

// A record's bytes in hex, sixteen a line, as xxd shows them.
template <typename Record>
std::string Hex(const Record& record) {
  const auto* bytes = reinterpret_cast<const unsigned char*>(&record);
  std::string hex;
  for (size_t i = 0; i < sizeof(Record); ++i) {
    if (i > 0) hex += i % 16 == 0 ? '\n' : ' ';
    absl::StrAppend(&hex, absl::Hex(bytes[i], absl::kZeroPad2));
  }
  return hex;
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

std::string NamesJson(const Graph& graph, std::string_view prefix) {
  std::string json = "{\"names\": [";
  std::vector<std::string> names =
      graph.NamesStartingWith(prefix, kSuggestions);
  for (size_t i = 0; i < names.size(); ++i) {
    absl::StrAppend(&json, i > 0 ? ", " : "", JsonString(names[i]));
  }
  return json + "]}";
}

std::string ErrorJson(std::string_view message) {
  return absl::StrCat("{\"error\": ", JsonString(message), "}");
}

absl::StatusOr<std::string> NodeJson(const Graph& graph, std::string_view id) {
  std::optional<uint32_t> node_id = ParseNumber(id, UINT32_MAX);
  if (!node_id || *node_id >= graph.node_count()) {
    return absl::NotFoundError(absl::StrCat("there's no node ", id));
  }
  NodeRecord node = graph.node(*node_id);
  std::string json = absl::StrCat(
      "{\"id\": ", *node_id, ", \"name\": ", JsonString(node.name),
      ", \"label\": ", JsonString(node.label), Where<NodeRecord>(*node_id),
      ", \"bytes\": ", JsonString(Hex(node)), ", \"chain\": [");
  // The chain from the node's first relationship to its last, which is the
  // order they're kept in: newest first.
  RelationshipId relationship_id = node.first_out;
  while (relationship_id != kNoRelationship) {
    RelationshipRecord relationship = graph.relationship(relationship_id);
    if (relationship_id != node.first_out) json += ", ";
    absl::StrAppend(
        &json, "{\"id\": ", relationship_id,
        ", \"type\": ", JsonString(relationship.type), ", \"properties\": ",
        PropertiesJson(graph.RelationshipProperties(relationship_id)),
        ", \"to\": ", relationship.to,
        ", \"to_name\": ", JsonString(graph.name(relationship.to)),
        ", \"to_label\": ", JsonString(graph.label(relationship.to)),
        Where<RelationshipRecord>(relationship_id), "}");
    relationship_id = relationship.next_out;
  }
  // Its properties' chain, newest first too, each record with where it's
  // kept.
  json += "], \"properties\": [";
  PropertyId property_id = node.first_property;
  while (property_id != kNoProperty) {
    PropertyRecord property = graph.property(property_id);
    if (property_id != node.first_property) json += ", ";
    absl::StrAppend(&json, "{\"id\": ", property_id,
                    ", \"key\": ", JsonString(property.key),
                    ", \"value\": ", ValueJson(ValueOf(property)),
                    Where<PropertyRecord>(property_id), "}");
    property_id = property.next;
  }
  return json + "]}";
}

absl::StatusOr<std::string> QueryJson(const Graph& graph,
                                      std::string_view text) {
  std::chrono::steady_clock::time_point start =
      std::chrono::steady_clock::now();
  uint64_t reads = graph.page_reads();
  absl::StatusOr<Query> query = Parse(text);
  if (!query.ok()) return query.status();
  absl::StatusOr<Plan> plan = PlanQuery(*query);
  if (!plan.ok()) return plan.status();
  if (query->explain) {
    return absl::StrCat("{\"plan\": ", JsonString(Explain(*plan)), "}");
  }
  Result result = Run(*plan, graph);
  std::chrono::duration<double, std::milli> took =
      std::chrono::steady_clock::now() - start;
  std::string json =
      absl::StrCat("{\"ms\": ", absl::SixDigits(took.count()),
                   ", \"pages\": ", graph.page_reads() - reads,
                   ", \"count\": ", result.rows.size(), ", \"columns\": [");
  for (size_t i = 0; i < result.columns.size(); ++i) {
    absl::StrAppend(&json, i > 0 ? ", " : "", JsonString(result.columns[i]));
  }
  json += "], \"rows\": [";
  for (size_t i = 0; i < result.rows.size() && i < kMaxRows; ++i) {
    if (i > 0) json += ", ";
    json += "[";
    for (size_t j = 0; j < result.rows[i].size(); ++j) {
      const Cell& cell = result.rows[i][j];
      absl::StrAppend(&json, j > 0 ? ", " : "", "{\"id\": ", cell.node,
                      ", \"value\": ", ValueJson(cell.value), "}");
    }
    json += "]";
  }

  // The nodes the rows return, each once, in the order they come, for the page
  // to draw, with the relationships from one of them to another.
  std::vector<NodeId> drawn;
  std::unordered_set<NodeId> seen;
  for (const std::vector<Cell>& row : result.rows) {
    for (const Cell& cell : row) {
      if (drawn.size() < kMaxDrawn && seen.insert(cell.node).second) {
        drawn.push_back(cell.node);
      }
    }
  }
  json += "], \"nodes\": [";
  for (size_t i = 0; i < drawn.size(); ++i) {
    absl::StrAppend(&json, i > 0 ? ", " : "", "{\"id\": ", drawn[i],
                    ", \"name\": ", JsonString(graph.name(drawn[i])),
                    ", \"label\": ", JsonString(graph.label(drawn[i])), "}");
  }
  json += "], \"relationships\": [";
  bool first = true;
  for (NodeId from : drawn) {
    RelationshipId id = graph.node(from).first_out;
    while (id != kNoRelationship) {
      RelationshipRecord relationship = graph.relationship(id);
      if (seen.contains(relationship.to)) {
        absl::StrAppend(&json, first ? "" : ", ", "{\"id\": ", id,
                        ", \"from\": ", from, ", \"to\": ", relationship.to,
                        ", \"type\": ", JsonString(relationship.type), "}");
        first = false;
      }
      id = relationship.next_out;
    }
  }
  return json + "]}";
}

}  // namespace smollergraph
