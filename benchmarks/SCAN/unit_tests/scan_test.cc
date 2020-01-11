#include "benchmarks/SCAN/scan.h"

#include <cmath>
#include <unordered_set>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ligra/graph.h"
#include "ligra/vertex.h"
#include "pbbslib/seq.h"

using testing::FloatEq;
using testing::IsEmpty;
using testing::Pair;
using testing::UnorderedElementsAre;

namespace {

// Make an undirected graph from a list of edges.
symmetric_graph<symmetric_vertex, pbbslib::empty> MakeGraph(
    const uintE num_vertices,
    const std::unordered_set<UndirectedEdge>& edges) {
  pbbs::sequence<std::tuple<uintE, uintE, pbbslib::empty>> edge_sequence(
      edges.size() * 2);
  auto edges_it{edges.cbegin()};
  for (size_t i = 0; i < edges.size(); i++) {
    edge_sequence[2 * i] =
      std::make_tuple(edges_it->from(), edges_it->to(), pbbs::empty{});
    edge_sequence[2 * i + 1] =
      std::make_tuple(edges_it->to(), edges_it->from(), pbbs::empty{});
    ++edges_it;
  }
  return sym_graph_from_edges(edge_sequence, num_vertices);
}

}  // namespace

TEST(ComputeStructuralSimilarities, NullGraph) {
  const size_t kNumVertices{0};
  const std::unordered_set<UndirectedEdge> kEdges{};

  auto graph{MakeGraph(kNumVertices, kEdges)};
  const scan::internal::StructuralSimilarities similarity_table{
    scan::internal::ComputeStructuralSimilarities(&graph)};
  const auto similarities{similarity_table.entries()};
  EXPECT_THAT(similarities, IsEmpty());
}

TEST(ComputeStructuralSimilarities, EmptyGraph) {
  const size_t kNumVertices{7};
  const std::unordered_set<UndirectedEdge> kEdges{};

  auto graph{MakeGraph(kNumVertices, kEdges)};
  const scan::internal::StructuralSimilarities similarity_table{
    scan::internal::ComputeStructuralSimilarities(&graph)};
  const auto similarities{similarity_table.entries()};
  EXPECT_THAT(similarities, IsEmpty());
}

TEST(ComputeStructuralSimilarities, BasicUsage) {
  // Graph diagram:
  //
  //     0 --- 1 -- 2 -- 5   6
  //           |   /|
  //           | /  |
  //           3 -- 4
  const size_t kNumVertices{7};
  const std::unordered_set<UndirectedEdge> kEdges{
    {0, 1},
    {1, 2},
    {1, 3},
    {2, 3},
    {2, 4},
    {2, 5},
    {3, 4},
  };

  auto graph{MakeGraph(kNumVertices, kEdges)};
  const scan::internal::StructuralSimilarities similarity_table{
    scan::internal::ComputeStructuralSimilarities(&graph)};
  const auto similarities{similarity_table.entries()};

  EXPECT_THAT(similarities.slice(), UnorderedElementsAre(
        Pair(Pair(0, 1), FloatEq(1.0 / sqrt(3))),
        Pair(Pair(1, 2), FloatEq(2.0 / sqrt(12))),
        Pair(Pair(1, 3), FloatEq(2.0 / sqrt(9))),
        Pair(Pair(2, 3), FloatEq(3.0 / sqrt(12))),
        Pair(Pair(2, 4), FloatEq(2.0 / sqrt(8))),
        Pair(Pair(2, 5), FloatEq(1.0 / sqrt(4))),
        Pair(Pair(3, 4), FloatEq(2.0 / sqrt(6)))));
}