#include "embed.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

namespace smollergraph {
namespace {

// Scales a vector to a length of one, unless it's all zeros.
void Normalise(Embedding& vector) {
  float sum = 0;
  for (float x : vector) sum += x * x;
  if (sum == 0) return;
  float scale = 1 / std::sqrt(sum);
  for (float& x : vector) x *= scale;
}

}  // namespace

std::vector<Embedding> FastRP(const Graph& graph, std::string_view type,
                              uint32_t seed) {
  size_t count = graph.node_count();
  // Each number of a node's first vector is 1 or -1, with a chance of one in
  // six each, and otherwise 0: a very sparse random projection, which keeps
  // the vectors of unrelated nodes close to right angles from each other.
  std::mt19937 random(seed);
  std::vector<Embedding> round(count);
  for (Embedding& vector : round) {
    for (float& x : vector) {
      uint32_t draw = random() % 6;
      x = draw == 0 ? 1.0f : draw == 1 ? -1.0f : 0.0f;
    }
  }
  // Each round goes through each node's neighbours, so they're read from
  // disk once, for all five.
  std::vector<std::vector<NodeId>> neighbours(count);
  for (NodeId node = 0; node < count; ++node) {
    neighbours[node] = graph.Neighbours(node, type, Direction::kBoth);
  }
  // Each round's weight in the embedding, as GDS's iterationWeights have them,
  // with two rounds more than its default of three, as a species' relatives in
  // a tree of taxa are four hops away, through the genus and the family.
  constexpr float kWeights[] = {0.0f, 1.0f, 1.0f, 1.0f, 1.0f};
  std::vector<Embedding> embeddings(count);
  for (float weight : kWeights) {
    std::vector<Embedding> next(count);
    for (NodeId node = 0; node < count; ++node) {
      for (NodeId neighbour : neighbours[node]) {
        for (size_t i = 0; i < kEmbeddingSize; ++i) {
          next[node][i] += round[neighbour][i];
        }
      }
      Normalise(next[node]);
      for (size_t i = 0; i < kEmbeddingSize; ++i) {
        embeddings[node][i] += weight * next[node][i];
      }
    }
    round = std::move(next);
  }
  return embeddings;
}

float Cosine(const Embedding& a, const Embedding& b) {
  float dot = 0;
  float a_squares = 0;
  float b_squares = 0;
  for (size_t i = 0; i < kEmbeddingSize; ++i) {
    dot += a[i] * b[i];
    a_squares += a[i] * a[i];
    b_squares += b[i] * b[i];
  }
  if (a_squares == 0 || b_squares == 0) return 0;
  return dot / std::sqrt(a_squares * b_squares);
}

std::vector<std::pair<NodeId, float>> MostSimilar(
    const std::vector<Embedding>& embeddings, NodeId node, size_t k) {
  std::vector<std::pair<NodeId, float>> similar;
  for (NodeId other = 0; other < embeddings.size(); ++other) {
    if (other == node) continue;
    similar.push_back({other, Cosine(embeddings[node], embeddings[other])});
  }
  // The k largest cosines go to the front, in order, and the rest stay
  // unsorted behind them.
  k = std::min(k, similar.size());
  std::partial_sort(
      similar.begin(), similar.begin() + k, similar.end(),
      [](const auto& a, const auto& b) { return a.second > b.second; });
  similar.resize(k);
  return similar;
}

}  // namespace smollergraph
