#ifndef SMOLLERGRAPH_LEXER_H_
#define SMOLLERGRAPH_LEXER_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"

namespace smollergraph {

// The kinds of token in our subset of Cypher.
enum class TokenKind {
  kWord,    // a keyword or a name: MATCH, a, name, EATS
  kString,  // a string, such as 'Red fox', without its quotes
  kNumber,  // a whole number, such as 2
  kSymbol,  // punctuation, such as ( or >, or a comparison, such as <=
  kEnd,     // after the last token
};

struct Token {
  TokenKind kind;
  std::string text;
  // Where the token starts in the query, for errors.
  size_t at;
};

// Splits a query into tokens, ending with a kEnd token. Returns an error that
// says what's wrong and where, at a character it doesn't know, or at a string
// with no closing quote.
absl::StatusOr<std::vector<Token>> Lex(std::string_view query);

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_LEXER_H_
