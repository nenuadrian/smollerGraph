#include "smollergraph/export.h"

#include <stdlib.h>

#include <fstream>
#include <sstream>
#include <string>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "smollergraph/graph.h"

namespace smollergraph {
namespace {

// The text of the file at `path`.
std::string Read(const std::string& path) {
  std::ifstream file(path);
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

TEST(ExportTest, WritesNodesAndRelationshipsForOtherPrograms) {
  Graph graph;
  NodeId lion = graph.AddNode("Lion");
  graph.SetLabel(lion, "Species");
  graph.SetProperty(lion, "status", std::string("vulnerable"));
  graph.AddRelationship(lion, "PARENT_TAXON", graph.AddNode("Panthera"));
  std::string path = ::testing::TempDir() + "export_test_XXXXXX";
  ASSERT_NE(mkdtemp(path.data()), nullptr);
  ASSERT_EQ(Export(graph, path), absl::OkStatus());
  EXPECT_EQ(Read(path + "/nodes.jsonl"),
            R"({"id": 0, "name": "Lion", "label": "Species", )"
            R"("properties": {"status": "vulnerable"}})"
            "\n"
            R"({"id": 1, "name": "Panthera", "label": "", "properties": {}})"
            "\n");
  EXPECT_EQ(Read(path + "/relationships.csv"),
            "from,to,type\n0,1,PARENT_TAXON\n");
}

}  // namespace
}  // namespace smollergraph
