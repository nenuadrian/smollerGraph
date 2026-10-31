#include "embed.h"

#include <stdlib.h>

#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "facts.h"
#include "graph.h"
#include "gtest/gtest.h"

namespace smollergraph {
namespace {

class EmbedTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Two genera of the order of carnivores, with two species each.
    std::istringstream facts(
        "(Lion)-[:PARENT_TAXON]->(Panthera)\n"
        "(Tiger)-[:PARENT_TAXON]->(Panthera)\n"
        "(Grey wolf)-[:PARENT_TAXON]->(Canis)\n"
        "(Golden jackal)-[:PARENT_TAXON]->(Canis)\n"
        "(Panthera)-[:PARENT_TAXON]->(Carnivora)\n"
        "(Canis)-[:PARENT_TAXON]->(Carnivora)\n");
    ASSERT_EQ(LoadFacts(facts, graph_), absl::OkStatus());
    embeddings_ = FastRP(graph_, "PARENT_TAXON", 42);
  }

  const Embedding& Of(std::string_view name) {
    return embeddings_[*graph_.Find(name)];
  }

  Graph graph_;
  std::vector<Embedding> embeddings_;
};

TEST_F(EmbedTest, GivesNodesWithTheSameNeighboursTheSameDirection) {
  // The lion and the tiger have the same neighbour, Panthera, so their
  // embeddings are the same, and the wolf's is further away.
  EXPECT_NEAR(Cosine(Of("Lion"), Of("Tiger")), 1, 1e-5);
  EXPECT_LT(Cosine(Of("Lion"), Of("Grey wolf")), 0.99);
  EXPECT_EQ(FastRP(graph_, "PARENT_TAXON", 42)[0], embeddings_[0]);
}

TEST_F(EmbedTest, FindsTheMostSimilarNodes) {
  std::vector<std::pair<NodeId, float>> similar =
      MostSimilar(embeddings_, *graph_.Find("Lion"), 2);
  ASSERT_EQ(similar.size(), 2);
  EXPECT_EQ(graph_.name(similar[0].first), "Tiger");
  EXPECT_GE(similar[0].second, similar[1].second);
}

TEST_F(EmbedTest, KeepsEmbeddingsInTheGraphsFolder) {
  std::string path = ::testing::TempDir() + "embed_test_XXXXXX";
  ASSERT_NE(mkdtemp(path.data()), nullptr);
  {
    Graph graph;
    ASSERT_EQ(graph.Open(path), absl::OkStatus());
    graph.AddRelationship(graph.AddNode("Lion"), "PARENT_TAXON",
                          graph.AddNode("Panthera"));
    // A graph without them has none to give.
    EXPECT_TRUE(graph.Embeddings().empty());
    std::vector<Embedding> embeddings = FastRP(graph, "PARENT_TAXON", 42);
    for (NodeId node = 0; node < embeddings.size(); ++node) {
      graph.SetEmbedding(node, embeddings[node]);
    }
  }
  Graph graph;
  ASSERT_EQ(graph.Open(path), absl::OkStatus());
  EXPECT_EQ(graph.Embeddings(), FastRP(graph, "PARENT_TAXON", 42));
}

}  // namespace
}  // namespace smollergraph
