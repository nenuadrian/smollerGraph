#ifndef SMOLLERGRAPH_TOKEN_STORE_H_
#define SMOLLERGRAPH_TOKEN_STORE_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "record_file.h"

namespace smollergraph {

// A number that stands for a name many records share, such as the label
// Species, so a record keeps four bytes in place of the name.
using TokenId = uint32_t;

// The token for no name, such as the label of a node that hasn't got one.
inline constexpr TokenId kNoToken = UINT32_MAX;

// A token's name on disk. Its id in the file is the token.
struct TokenRecord {
  char name[32];
};

// Names and the tokens that stand for them, kept in a file of records, and in
// memory both ways, as there are only a few of them. Neo4j keeps its labels
// and relationship types in stores like this one.
class TokenStore {
 public:
  // The longest name a token can stand for.
  static constexpr size_t kMaxName = sizeof(TokenRecord::name) - 1;

  // Opens the store in the file at `path`, creating it when it isn't there.
  absl::Status Open(const std::string& path);
  absl::Status OpenTemporary();
  void Flush() { file_.Flush(); }
  uint64_t page_reads() const { return file_.page_reads(); }
  uint64_t page_writes() const { return file_.page_writes(); }

  // Returns the token for `name`, adding it the first time. The name has to be
  // at most kMaxName bytes long.
  TokenId Add(std::string_view name);
  // Returns the token for `name`, if the store has one.
  std::optional<TokenId> Find(std::string_view name) const;
  // Returns the name `token` stands for, or nothing for kNoToken.
  std::string Name(TokenId token) const {
    return token == kNoToken ? "" : names_[token];
  }

 private:
  // Reads the file's names into memory, once it's open.
  void Load();

  RecordFile<TokenRecord> file_;
  // Each token's name, and each name's token.
  std::vector<std::string> names_;
  std::map<std::string, TokenId, std::less<>> tokens_;
};

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_TOKEN_STORE_H_
