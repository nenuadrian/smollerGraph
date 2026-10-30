#include "token_store.h"

#include <algorithm>
#include <cstring>
#include <optional>
#include <string>

#include "absl/status/status.h"

namespace smollergraph {

absl::Status TokenStore::Open(const std::string& path) {
  absl::Status status = file_.Open(path);
  if (status.ok()) Load();
  return status;
}

absl::Status TokenStore::OpenTemporary() {
  absl::Status status = file_.OpenTemporary();
  if (status.ok()) Load();
  return status;
}

void TokenStore::Load() {
  names_.clear();
  tokens_.clear();
  for (TokenId token = 0; token < file_.count(); ++token) {
    names_.emplace_back(file_.Read(token).name);
    tokens_.emplace(names_.back(), token);
  }
}

TokenId TokenStore::Add(std::string_view name) {
  if (std::optional<TokenId> found = Find(name)) return *found;
  TokenRecord record;
  std::memset(record.name, 0, sizeof(record.name));
  std::memcpy(record.name, name.data(), std::min(name.size(), kMaxName));
  TokenId token = file_.Append(record);
  names_.emplace_back(record.name);
  tokens_.emplace(names_.back(), token);
  return token;
}

std::optional<TokenId> TokenStore::Find(std::string_view name) const {
  auto found = tokens_.find(name);
  if (found == tokens_.end()) return std::nullopt;
  return found->second;
}

}  // namespace smollergraph
