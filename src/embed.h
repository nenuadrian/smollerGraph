#ifndef SMOLLERGRAPH_EMBED_H_
#define SMOLLERGRAPH_EMBED_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "graph.h"

namespace smollergraph {

// How many numbers a node's embedding has.
inline constexpr size_t kEmbeddingSize = 256;

// A node's embedding: a vector of numbers that stands for where the node is in
// the graph, so that nodes with similar neighbourhoods get vectors that point
// in similar directions.
using Embedding = std::array<float, kEmbeddingSize>;

// Returns an embedding for each node, in the order of their ids, computed with
// FastRP, as Neo4j's Graph Data Science library does: a sparse random vector
// for each node, then five rounds, in each of which a node's vector is the sum
// of its neighbours' vectors from the round before, along relationships of
// `type` followed both ways, scaled to a length of one. The embedding adds up
// the rounds from the second on. A seed gives the same vectors each time.
std::vector<Embedding> FastRP(const Graph& graph, std::string_view type,
                              uint32_t seed);

// How alike two embeddings are: the cosine of the angle between them, 1 when
// they point the same way, 0 at right angles, and -1 when they're opposite.
float Cosine(const Embedding& a, const Embedding& b);

// Returns the `k` nodes whose embeddings are most like `node`'s, with their
// cosines, the most alike first. It compares `node` with each of the others.
std::vector<std::pair<NodeId, float>> MostSimilar(
    const std::vector<Embedding>& embeddings, NodeId node, size_t k);

}  // namespace smollergraph

#endif  // SMOLLERGRAPH_EMBED_H_
