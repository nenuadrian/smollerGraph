#include "facts.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "parser.h"

namespace smollergraph {
namespace {

// A node or a relationship type in a fact, with a node's label after a colon,
// and the properties in braces after that, as in "Red fox:Animal {legs: 4}".
struct Part {
  std::string_view text;
  std::string_view label;
  std::vector<Property> properties;
};

// Reads a part of a fact, and returns false when it can't. Its braces are
// written as in Cypher, so our parser reads them. The text has to be at most
// `max` bytes, without brackets, a label has to fit in a node's record, and
// the keys and strings in property records.
bool ReadPart(std::string_view text, size_t max, Part& part) {
  part = {text, {}, {}};
  if (size_t open = text.find(" {"); open != std::string_view::npos) {
    absl::StatusOr<std::vector<Property>> properties =
        ParseProperties(text.substr(open + 1));
    if (!properties.ok()) return false;
    part = {text.substr(0, open), {}, *std::move(properties)};
  }
  if (size_t colon = part.text.find(':'); colon != std::string_view::npos) {
    part.label = part.text.substr(colon + 1);
    part.text = part.text.substr(0, colon);
    if (part.label.empty() || part.label.size() > Graph::kMaxLabel) {
      return false;
    }
  }
  if (part.text.empty() || part.text.size() > max ||
      part.text.find_first_of("()[]{}") != std::string_view::npos) {
    return false;
  }
  for (const Property& property : part.properties) {
    const std::string* string = std::get_if<std::string>(&property.value);
    // A node's name is in its record, so it isn't a property to set.
    if (property.key == "name" || property.key.size() > Graph::kMaxKey ||
        (string != nullptr && string->size() > Graph::kMaxString)) {
      return false;
    }
  }
  return true;
}

NodeId AddWithProperties(Graph& graph, const Part& part) {
  NodeId node = graph.AddNode(part.text);
  if (!part.label.empty()) graph.SetLabel(node, part.label);
  for (const Property& property : part.properties) {
    graph.SetProperty(node, property.key, property.value);
  }
  return node;
}

}  // namespace

absl::Status LoadFacts(std::istream& in, Graph& graph) {
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line.starts_with('#')) continue;
    if (!line.starts_with('(') || !line.ends_with(')')) {
      return absl::InvalidArgumentError(line);
    }
    // What's inside the first and the last bracket.
    std::string_view text = std::string_view(line).substr(1, line.size() - 2);
    size_t open = text.find(")-[:");
    size_t close = text.find("]->(");
    // A fact is a node on its own, or a node, a relationship and a node.
    std::vector<Part> parts;
    bool read = false;
    if (open == std::string_view::npos && close == std::string_view::npos) {
      parts.resize(1);
      read = ReadPart(text, Graph::kMaxName, parts[0]);
    } else if (open != std::string_view::npos &&
               close != std::string_view::npos && open < close) {
      parts.resize(3);
      read = ReadPart(text.substr(0, open), Graph::kMaxName, parts[0]) &&
             ReadPart(text.substr(open + 4, close - open - 4), Graph::kMaxType,
                      parts[1]) &&
             ReadPart(text.substr(close + 4), Graph::kMaxName, parts[2]);
    }
    if (!read) return absl::InvalidArgumentError(line);

    NodeId from = AddWithProperties(graph, parts[0]);
    if (parts.size() == 1) continue;
    NodeId to = AddWithProperties(graph, parts[2]);
    RelationshipId relationship =
        graph.AddRelationship(from, parts[1].text, to);
    for (const Property& property : parts[1].properties) {
      graph.SetRelationshipProperty(relationship, property.key, property.value);
    }
  }
  return absl::OkStatus();
}

}  // namespace smollergraph
