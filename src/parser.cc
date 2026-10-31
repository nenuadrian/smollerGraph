#include "parser.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "lexer.h"

namespace smollergraph {
namespace {

// The most hops one relationship in a pattern can stand for.
constexpr int kMaxHops = 10;
// The operators a comparison in WHERE can have.
constexpr std::string_view kOperators[] = {"=", "<>", "<", "<=", ">", ">="};

bool EqualsIgnoringCase(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::toupper(static_cast<unsigned char>(a[i])) !=
        std::toupper(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}

// A recursive descent parser: a function for each part of the query, which
// reads that part's tokens, and calls the functions for the parts inside it.
// Each returns false, with the parser's status filled in, when the tokens
// don't fit.
class Parser {
 public:
  explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

  // What's wrong with the query, once a part has failed.
  const absl::Status& status() const { return status_; }

  // query := [EXPLAIN] MATCH [variable =] (pattern | shortestPath(pattern))
  //          [WHERE comparison (AND comparison)*]
  //          RETURN [DISTINCT] item (, item)*
  // pattern := node (relationship node)*
  bool ParseQuery(Query& query) {
    query.explain = Keyword("EXPLAIN");
    if (!ExpectKeyword("MATCH")) return false;
    // A word and an equals sign name the path that follows.
    if (Peek().kind == TokenKind::kWord && IsSymbolAt(1, "=")) {
      query.path = Next().text;
      Next();
      query.shortest = Keyword("shortestPath");
      if (query.shortest && !ExpectSymbol("(")) return false;
    }
    NodePattern node;
    if (!ParseNode(node)) return false;
    query.nodes.push_back(node);
    while (IsSymbol("-") || IsSymbol("<")) {
      RelationshipPattern relationship;
      if (!ParseRelationship(relationship) || !ParseNode(node)) return false;
      query.relationships.push_back(relationship);
      query.nodes.push_back(node);
    }
    if (query.shortest && !ExpectSymbol(")")) return false;
    if (Keyword("WHERE")) {
      do {
        Comparison comparison;
        if (!ParseComparison(comparison)) return false;
        query.where.push_back(std::move(comparison));
      } while (Keyword("AND"));
    }
    if (!ExpectKeyword("RETURN")) return false;
    query.distinct = Keyword("DISTINCT");
    do {
      ReturnItem item;
      if (!ParseReturnItem(item)) return false;
      query.returns.push_back(std::move(item));
    } while (Symbol(","));
    if (Peek().kind != TokenKind::kEnd) return Fail("the end of the query");
    return true;
  }

  // Properties on their own, as the facts we load have them.
  bool ParsePropertiesAlone(std::vector<Property>& properties) {
    if (!ParseProperties(properties)) return false;
    if (Peek().kind != TokenKind::kEnd) {
      return Fail("the end of the properties");
    }
    return true;
  }

 private:
  // node := ( [variable] [: label] [properties] )
  bool ParseNode(NodePattern& node) {
    node = {};
    if (!ExpectSymbol("(")) return false;
    if (Peek().kind == TokenKind::kWord) node.variable = Next().text;
    if (Symbol(":")) {
      if (Peek().kind != TokenKind::kWord) return Fail("a label");
      node.label = Next().text;
    }
    if (IsSymbol("{") && !ParseProperties(node.properties)) return false;
    return ExpectSymbol(")");
  }

  // relationship := - [ : type [* [hops | WSHORTEST(key)]] ] - [>]
  //               | < - [ : type [* [hops | WSHORTEST(key)]] ] -
  bool ParseRelationship(RelationshipPattern& relationship) {
    if (Symbol("<")) relationship.direction = Direction::kIn;
    if (!ExpectSymbol("-") || !ExpectSymbol("[") || !ExpectSymbol(":")) {
      return false;
    }
    if (Peek().kind != TokenKind::kWord) return Fail("a relationship type");
    relationship.type = Next().text;
    if (Symbol("*")) {
      const std::string& text = Peek().text;
      auto [end, failed] = std::from_chars(
          text.data(), text.data() + text.size(), relationship.hops);
      // A star with no number after it stands for any number of hops, and
      // WSHORTEST after it names the weight a cheapest path adds up.
      if (Peek().kind != TokenKind::kNumber) {
        relationship.hops = kAnyHops;
        if (Keyword("WSHORTEST")) {
          if (!ExpectSymbol("(")) return false;
          if (Peek().kind != TokenKind::kWord) return Fail("a key");
          relationship.weight = Next().text;
          if (!ExpectSymbol(")")) return false;
        }
      } else if (failed != std::errc() || relationship.hops < 1 ||
                 relationship.hops > kMaxHops) {
        return Fail("a number of hops from 1 to " + std::to_string(kMaxHops));
      } else {
        Next();
      }
    }
    if (!ExpectSymbol("]") || !ExpectSymbol("-")) return false;
    // An arrow on the left needs none on the right, and a relationship with
    // no arrow at all goes either way.
    if (relationship.direction == Direction::kOut && !Symbol(">")) {
      relationship.direction = Direction::kBoth;
    }
    return true;
  }

  // properties := { key : value (, key : value)* }
  bool ParseProperties(std::vector<Property>& properties) {
    if (!ExpectSymbol("{")) return false;
    do {
      Property property;
      if (Peek().kind != TokenKind::kWord) return Fail("a key");
      property.key = Next().text;
      if (!ExpectSymbol(":") || !ParseValue(property.value)) return false;
      properties.push_back(std::move(property));
    } while (Symbol(","));
    return ExpectSymbol("}");
  }

  // value := string | number
  bool ParseValue(Value& value) {
    if (Peek().kind == TokenKind::kString) {
      value = Next().text;
      return true;
    }
    const std::string& text = Peek().text;
    int64_t number = 0;
    auto [end, failed] =
        std::from_chars(text.data(), text.data() + text.size(), number);
    if (Peek().kind != TokenKind::kNumber || failed != std::errc()) {
      return Fail("a string or a whole number");
    }
    Next();
    value = number;
    return true;
  }

  // comparison := lookup operator value
  bool ParseComparison(Comparison& comparison) {
    if (!ParseLookup(comparison.property)) return false;
    if (Peek().kind != TokenKind::kSymbol ||
        std::find(std::begin(kOperators), std::end(kOperators), Peek().text) ==
            std::end(kOperators)) {
      return Fail("a comparison");
    }
    comparison.op = Next().text;
    return ParseValue(comparison.value);
  }

  // lookup := variable . key
  bool ParseLookup(PropertyLookup& lookup) {
    if (Peek().kind != TokenKind::kWord) return Fail("a variable");
    lookup.variable = Next().text;
    if (!ExpectSymbol(".")) return false;
    if (Peek().kind != TokenKind::kWord) return Fail("a key");
    lookup.key = Next().text;
    return true;
  }

  // item := variable [. key] | (length | cost) ( variable )
  bool ParseReturnItem(ReturnItem& item) {
    if (Peek().kind == TokenKind::kWord && IsSymbolAt(1, "(")) {
      if (Keyword("length")) {
        item.function = "length";
      } else if (Keyword("cost")) {
        item.function = "cost";
      } else {
        return Fail("length or cost");
      }
      if (!ExpectSymbol("(")) return false;
      if (Peek().kind != TokenKind::kWord) return Fail("a variable");
      item.variable = Next().text;
      return ExpectSymbol(")");
    }
    if (Peek().kind != TokenKind::kWord) return Fail("a variable");
    item.variable = Next().text;
    // A variable on its own returns all of it, which our subset allows for a
    // path, as a path has no properties.
    if (!Symbol(".")) return true;
    if (Peek().kind != TokenKind::kWord) return Fail("a key");
    item.key = Next().text;
    return true;
  }

  const Token& Peek() const { return tokens_[next_]; }
  const Token& Next() { return tokens_[next_++]; }

  bool IsSymbol(std::string_view symbol) const { return IsSymbolAt(0, symbol); }
  // Whether the token `ahead` of the next one is this symbol. The last token
  // is the end, so looking past it finds the end.
  bool IsSymbolAt(size_t ahead, std::string_view symbol) const {
    const Token& token = tokens_[std::min(next_ + ahead, tokens_.size() - 1)];
    return token.kind == TokenKind::kSymbol && token.text == symbol;
  }
  // Reads a symbol, if it's the next token.
  bool Symbol(std::string_view symbol) {
    if (!IsSymbol(symbol)) return false;
    ++next_;
    return true;
  }
  // Reads a keyword, if it's the next token. Cypher's keywords can be written
  // in any case, so match is MATCH.
  bool Keyword(std::string_view keyword) {
    if (Peek().kind != TokenKind::kWord ||
        !EqualsIgnoringCase(Peek().text, keyword)) {
      return false;
    }
    ++next_;
    return true;
  }
  bool ExpectSymbol(std::string_view symbol) {
    return Symbol(symbol) || Fail(std::string(symbol));
  }
  bool ExpectKeyword(std::string_view keyword) {
    return Keyword(keyword) || Fail(std::string(keyword));
  }

  // Says what was expected, where, and what was there.
  bool Fail(const std::string& expected) {
    const Token& token = Peek();
    status_ = absl::InvalidArgumentError(
        absl::StrCat("expected ", expected, " at ", token.at, ", not ",
                     token.kind == TokenKind::kEnd ? "the end" : token.text));
    return false;
  }

  std::vector<Token> tokens_;
  size_t next_ = 0;
  absl::Status status_;
};

}  // namespace

absl::StatusOr<Query> Parse(std::string_view text) {
  absl::StatusOr<std::vector<Token>> tokens = Lex(text);
  if (!tokens.ok()) return tokens.status();
  Parser parser(*std::move(tokens));
  Query query;
  if (!parser.ParseQuery(query)) return parser.status();
  return query;
}

absl::StatusOr<std::vector<Property>> ParseProperties(std::string_view text) {
  absl::StatusOr<std::vector<Token>> tokens = Lex(text);
  if (!tokens.ok()) return tokens.status();
  Parser parser(*std::move(tokens));
  std::vector<Property> properties;
  if (!parser.ParsePropertiesAlone(properties)) return parser.status();
  return properties;
}

}  // namespace smollergraph
