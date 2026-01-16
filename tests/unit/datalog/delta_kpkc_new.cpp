/*
 * Copyright (C) 2025 Dominik Drexler
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "tyr/datalog/delta_kpkc_new.hpp"

#include "tyr/common/formatter.hpp"
#include "tyr/formalism/formalism.hpp"
#include "tyr/planning/planning.hpp"

#include <gtest/gtest.h>

namespace d = tyr::datalog;
namespace p = tyr::planning;
namespace x = tyr::datalog::delta_kpkc2;

namespace tyr::tests
{

inline std::vector<x::Vertex> V(std::initializer_list<size_t> xs)
{
    auto out = std::vector<x::Vertex> {};
    out.reserve(xs.size());
    for (auto x : xs)
        out.emplace_back(x);
    return out;
}

inline boost::dynamic_bitset<> B(size_t n, std::initializer_list<size_t> bits)
{
    boost::dynamic_bitset<> bs(n);
    for (auto b : bits)
    {
        assert(b < n);
        bs.set(b);
    }
    return bs;
}

inline x::VertexSet VS(size_t k, size_t n, std::initializer_list<size_t> part_bits, std::initializer_list<size_t> vert_bits)
{
    return x::VertexSet {
        .partition_bits = B(k, part_bits),
        .partition_count = part_bits.size(),
        .vertex_bits = B(n, vert_bits),
        .vertex_count = vert_bits.size(),
    };
}

inline auto enumerate_new_cliques(x::DeltaKPKC& kpkc)
{
    auto result = std::vector<std::vector<x::Vertex>> {};

    bool has_new_head = false;

    kpkc.for_each_new_head_clique(
        [&](auto&& head_clique)
        {
            has_new_head = true;

            result.push_back(head_clique);

            kpkc.for_each_rule_clique([&](auto&& rule_clique) { result.push_back(rule_clique); });
        });

    if (!has_new_head)
    {
        kpkc.for_each_head_clique([&](auto&& head_clique) { kpkc.for_each_new_rule_clique([&](auto&& rule_clique) { result.push_back(rule_clique); }); });
    }

    return result;
}

inline auto enumerate_all_cliques(x::DeltaKPKC& kpkc)
{
    auto result = std::vector<std::vector<x::Vertex>> {};

    kpkc.for_each_head_clique(
        [&](auto&& head_clique)
        {
            result.push_back(head_clique);

            kpkc.for_each_rule_clique([&](auto&& rule_clique) { result.push_back(rule_clique); });
        });

    return result;
}

inline auto allocate_workspace(const x::ConstGraph& graph)
{
    auto workspace = x::Workspace {};

    workspace.compatible_vertices.resize(graph.k);
    for (uint_t i = 0; i < graph.k; ++i)
    {
        workspace.compatible_vertices[i].resize(graph.k);
        for (uint_t j = 0; j < graph.k; ++j)
            workspace.compatible_vertices[i][j].resize(graph.partitions[j].size());
    }
    workspace.partition_bits.resize(graph.k);
    workspace.partial_solution.reserve(graph.k);

    return workspace;
}

/**
 * Head arity 0 / Remainder arity 0
 */

TEST(TyrTests, TyrDatalogDeltaKPKC_0_0)
{
    auto const_graph = x::ConstGraph {
        .num_vertices = 0,
        .k = 0,
        .partitions = {},
        .vertex_to_partition = {},
        .head = {},
        .non_head = {},
        .full = {},
    };
    auto delta_graph = x::Graph { .vertices = {}, .adjacency_matrix = {} };
    auto full_graph = x::Graph { .vertices = {}, .adjacency_matrix = {} };

    auto workspace = allocate_workspace(const_graph);

    auto kpkc = x::DeltaKPKC(std::move(const_graph), std::move(delta_graph), std::move(full_graph), std::move(workspace));

    auto new_cliques = enumerate_new_cliques(kpkc);

    EXPECT_EQ(new_cliques.size(), 1);
    EXPECT_EQ(new_cliques, (std::vector<std::vector<x::Vertex>> { V({}) }));

    auto all_cliques = enumerate_all_cliques(kpkc);

    EXPECT_EQ(all_cliques.size(), 1);
    EXPECT_EQ(all_cliques, (std::vector<std::vector<x::Vertex>> { V({}) }));
}

/**
 * Head arity 1 / Remainder arity 0
 */

TEST(TyrTests, TyrDatalogDeltaKPKC_1_0)
{
    auto const_graph = x::ConstGraph {
        .num_vertices = 2,
        .k = 1,
        .partitions = { V({ 0, 1 }) },
        .vertex_to_partition = { 0, 0 },
        .head = VS(1, 2, { 0 }, { 0, 1 }),
        .non_head = {},
        .full = VS(1, 2, { 0 }, { 0, 1 }),
    };
    auto delta_graph = x::Graph { .vertices = B(2, { 0 }), .adjacency_matrix = {} };
    auto full_graph = x::Graph { .vertices = B(2, { 0, 1 }), .adjacency_matrix = {} };

    auto workspace = allocate_workspace(const_graph);

    auto kpkc = x::DeltaKPKC(std::move(const_graph), std::move(delta_graph), std::move(full_graph), std::move(workspace));

    auto new_cliques = enumerate_new_cliques(kpkc);

    EXPECT_EQ(new_cliques.size(), 1);
    EXPECT_EQ(new_cliques, (std::vector<std::vector<x::Vertex>> { V({ 0 }) }));

    auto all_cliques = enumerate_all_cliques(kpkc);

    EXPECT_EQ(all_cliques.size(), 2);
    EXPECT_EQ(all_cliques, (std::vector<std::vector<x::Vertex>> { V({ 0 }), V({ 1 }) }));
}

/**
 * Head arity 0 / Remainder arity 1
 */

TEST(TyrTests, TyrDatalogDeltaKPKC_0_1)
{
    auto const_graph = x::ConstGraph {
        .num_vertices = 2,
        .k = 1,
        .partitions = { V({ 0, 1 }) },
        .vertex_to_partition = { 0, 0 },
        .head = {},
        .non_head = VS(1, 2, { 0 }, { 0, 1 }),
        .full = VS(1, 2, { 0 }, { 0, 1 }),
    };
    auto delta_graph = x::Graph { .vertices = B(2, { 0 }), .adjacency_matrix = {} };
    auto full_graph = x::Graph { .vertices = B(2, { 0, 1 }), .adjacency_matrix = {} };

    auto workspace = allocate_workspace(const_graph);

    auto kpkc = x::DeltaKPKC(std::move(const_graph), std::move(delta_graph), std::move(full_graph), std::move(workspace));

    auto new_cliques = enumerate_new_cliques(kpkc);

    EXPECT_EQ(new_cliques.size(), 1);
    EXPECT_EQ(new_cliques, (std::vector<std::vector<x::Vertex>> { V({ 0 }) }));

    auto all_cliques = enumerate_all_cliques(kpkc);

    EXPECT_EQ(all_cliques.size(), 2);
    EXPECT_EQ(all_cliques, (std::vector<std::vector<x::Vertex>> { V({ 0 }), V({ 1 }) }));
}

/**
 * Head arity 1 / Remainder arity 1
 */

TEST(TyrTests, TyrDatalogDeltaKPKCNew_1_1) {}

TEST(TyrTests, TyrDatalogDeltaKPKCExhaustive_1_1) {}

/**
 * Head arity 2 / Remainder arity 1
 */

TEST(TyrTests, TyrDatalogDeltaKPKCNew_2_1) {}

TEST(TyrTests, TyrDatalogDeltaKPKCExhaustive_2_1) {}

/**
 * Head arity 1 / Remainder arity 2
 */

TEST(TyrTests, TyrDatalogDeltaKPKCNew_1_2) {}

TEST(TyrTests, TyrDatalogDeltaKPKCExhaustive_1_2) {}

/**
 * Head arity 2 / Remainder arity 2
 */

TEST(TyrTests, TyrDatalogDeltaKPKCNew_2_2) {}

TEST(TyrTests, TyrDatalogDeltaKPKCExhaustive_2_2) {}

/**
 * Head arity 3 / Remainder arity 2
 */

TEST(TyrTests, TyrDatalogDeltaKPKCNew_3_2) {}

TEST(TyrTests, TyrDatalogDeltaKPKCExhaustive_3_2) {}

/**
 * Head arity 2 / Remainder arity 3
 */

TEST(TyrTests, TyrDatalogDeltaKPKCNew_2_3) {}

TEST(TyrTests, TyrDatalogDeltaKPKCExhaustive_2_3) {}

/**
 * Head arity 3 / Remainder arity 3
 */

TEST(TyrTests, TyrDatalogDeltaKPKCNew_3_3) {}

TEST(TyrTests, TyrDatalogDeltaKPKCExhaustive_3_3) {}

}