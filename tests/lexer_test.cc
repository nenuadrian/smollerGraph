#include "lexer.h"

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "gtest/gtest.h"

namespace smollergraph {
namespace {

// The tokens' texts, and their kinds, as one string, so a test can check them
// at a glance.
std::string Describe(const std::vector<Token>& tokens) {
  std::string described;
  for (const Token& token : tokens) {
    switch (token.kind) {
      case TokenKind::kWord:
        described += "word:" + token.text + " ";
        break;
      case TokenKind::kString:
        described += "string:" + token.text + " ";
        break;
      case TokenKind::kNumber:
        described += "number:" + token.text + " ";
        break;
      case TokenKind::kSymbol:
        described += token.text + " ";
        break;
      case TokenKind::kEnd:
        described += "end";
        break;
    }
  }
  return described;
}

TEST(LexerTest, SplitsAQueryIntoTokens) {
  absl::StatusOr<std::vector<Token>> tokens =
      Lex("MATCH (a {name: 'Red fox'})-[:EATS*2]->(c) RETURN c.name");
  ASSERT_TRUE(tokens.ok()) << tokens.status();
  EXPECT_EQ(Describe(*tokens),
            "word:MATCH ( word:a { word:name : string:Red fox } ) - [ : "
            "word:EATS * number:2 ] - > ( word:c ) word:RETURN word:c . "
            "word:name end");
}

TEST(LexerTest, ReadsComparisons) {
  absl::StatusOr<std::vector<Token>> tokens =
      Lex("WHERE a.legs >= 4 AND a.diet <> 'herbivore'");
  ASSERT_TRUE(tokens.ok()) << tokens.status();
  EXPECT_EQ(Describe(*tokens),
            "word:WHERE word:a . word:legs >= number:4 word:AND word:a . "
            "word:diet <> string:herbivore end");
}

TEST(LexerTest, SaysWhatItCannotRead) {
  EXPECT_EQ(Lex("MATCH (a {name: 'Red fox})").status().message(),
            "no closing quote for the string at 16");
  EXPECT_EQ(Lex("MATCH (a) RETURN a;").status().message(),
            "unexpected ; at 18");
}

}  // namespace
}  // namespace smollergraph
