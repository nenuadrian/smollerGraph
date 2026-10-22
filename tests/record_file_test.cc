#include "record_file.h"

#include <cstdint>
#include <cstdio>
#include <string>

#include "absl/status/status.h"
#include "gtest/gtest.h"

namespace smollergraph {
namespace {

struct Pair {
  uint32_t a;
  uint32_t b;
};

TEST(RecordFileTest, AppendsAcrossPages) {
  RecordFile<Pair> file;
  ASSERT_EQ(file.OpenTemporary(), absl::OkStatus());
  // Enough records to fill the first page and start a second.
  for (uint32_t i = 0; i < RecordFile<Pair>::kPerPage + 10; ++i) {
    EXPECT_EQ(file.Append({.a = i, .b = i * 2}), i);
  }
  Pair last = file.Read(RecordFile<Pair>::kPerPage + 9);
  EXPECT_EQ(last.a, RecordFile<Pair>::kPerPage + 9);
  EXPECT_EQ(file.Read(3).b, 6);
}

TEST(RecordFileTest, KeepsItsCountInTheHeader) {
  std::string path = ::testing::TempDir() + "record_file_test";
  std::remove(path.c_str());
  {
    RecordFile<Pair> file;
    ASSERT_EQ(file.Open(path), absl::OkStatus());
    file.Append({.a = 1, .b = 2});
    file.Append({.a = 3, .b = 4});
  }
  RecordFile<Pair> file;
  ASSERT_EQ(file.Open(path), absl::OkStatus());
  EXPECT_EQ(file.count(), 2);
  EXPECT_EQ(file.Read(1).a, 3);
}

}  // namespace
}  // namespace smollergraph
