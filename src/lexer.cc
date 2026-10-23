#include "lexer.h"

#include <cctype>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

namespace smollergraph {
namespace {

bool IsWordStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool IsWordPart(char c) {
  return IsWordStart(c) || std::isdigit(static_cast<unsigned char>(c));
}

bool IsDigit(char c) { return std::isdigit(static_cast<unsigned char>(c)); }

}  // namespace

absl::StatusOr<std::vector<Token>> Lex(std::string_view query) {
  std::vector<Token> tokens;
  size_t i = 0;
  while (i < query.size()) {
    char c = query[i];
    size_t start = i;
    if (std::isspace(static_cast<unsigned char>(c))) {
      ++i;
    } else if (IsWordStart(c)) {
      while (i < query.size() && IsWordPart(query[i])) ++i;
      tokens.push_back({TokenKind::kWord,
                        std::string(query.substr(start, i - start)), start});
    } else if (IsDigit(c)) {
      while (i < query.size() && IsDigit(query[i])) ++i;
      tokens.push_back({TokenKind::kNumber,
                        std::string(query.substr(start, i - start)), start});
    } else if (c == '\'' || c == '"') {
      // A string runs to the next quote like the one it started with.
      size_t end = query.find(c, i + 1);
      if (end == std::string_view::npos) {
        return absl::InvalidArgumentError(
            absl::StrCat("no closing quote for the string at ", start));
      }
      tokens.push_back({TokenKind::kString,
                        std::string(query.substr(i + 1, end - i - 1)), start});
      i = end + 1;
    } else if (std::string_view pair = query.substr(i, 2);
               pair == "<=" || pair == ">=" || pair == "<>") {
      // Two characters that make one comparison.
      tokens.push_back({TokenKind::kSymbol, std::string(pair), start});
      i += 2;
    } else if (std::string_view("()[]{}:,.-<>*=").find(c) !=
               std::string_view::npos) {
      tokens.push_back({TokenKind::kSymbol, std::string(1, c), start});
      ++i;
    } else {
      return absl::InvalidArgumentError(
          absl::StrCat("unexpected ", std::string_view(&c, 1), " at ", start));
    }
  }
  tokens.push_back({TokenKind::kEnd, "", query.size()});
  return tokens;
}

}  // namespace smollergraph
