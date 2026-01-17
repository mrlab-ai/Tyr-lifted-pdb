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

#ifndef TYR_DATALOG_KPKC_HPP_
#define TYR_DATALOG_KPKC_HPP_

#include "tyr/common/formatter.hpp"
#include "tyr/datalog/declarations.hpp"

#include <boost/dynamic_bitset.hpp>
#include <cstddef>
#include <iostream>
#include <vector>

namespace tyr::datalog::delta_kpkc
{
struct Vertex
{
    uint_t index;

    constexpr Vertex() : index(std::numeric_limits<uint_t>::max()) {}
    constexpr explicit Vertex(uint_t i) : index(i) {}

    friend constexpr bool operator==(Vertex lhs, Vertex rhs) noexcept { return lhs.index == rhs.index; }
};

struct Edge
{
    Vertex src;
    Vertex dst;

    constexpr Edge() : src(), dst() {}
    constexpr Edge(Vertex u, Vertex v) : src(u.index < v.index ? u : v), dst(u.index < v.index ? v : u) {}

    friend constexpr bool operator==(Edge lhs, Edge rhs) noexcept { return lhs.src == rhs.src && lhs.dst == rhs.dst; }
};

struct ConstGraph
{
    /// Meta
    size_t num_vertices;
    size_t k;
    /// Vertex partitioning with continuous vertex indices [[0,1,2],[3,4],[5,6]]
    std::vector<boost::dynamic_bitset<>> partition_masks;  ///< Dimensions K x V
    std::vector<uint_t> vertex_to_partition;
};

struct Graph
{
    /// Vertices
    boost::dynamic_bitset<> vertices;  ///< Dimensions V
    /// Edges
    std::vector<boost::dynamic_bitset<>> adjacency_matrix;  ///< Dimensions V x V

    void reset() noexcept
    {
        vertices.reset();
        for (auto& bitset : adjacency_matrix)
            bitset.reset();
    }

    bool contains(Vertex vertex) const { return vertices.test(vertex.index); }
    bool contains(Edge edge) const { return adjacency_matrix[edge.src.index].test(edge.dst.index); }

    struct VertexIterator
    {
    private:
        const Graph* m_graph;
        boost::dynamic_bitset<>::size_type i;

    private:
        void advance_to_next()
        {
            if (i != boost::dynamic_bitset<>::npos)
                i = m_graph->vertices.find_next(i);
        }

    public:
        using difference_type = std::ptrdiff_t;
        using value_type = Vertex;
        using iterator_category = std::input_iterator_tag;
        using iterator_concept = std::input_iterator_tag;

        VertexIterator() : m_graph(nullptr), i(boost::dynamic_bitset<>::npos) {}
        VertexIterator(const Graph& graph, bool begin) : m_graph(&graph), i(begin ? graph.vertices.find_first() : boost::dynamic_bitset<>::npos) {}

        value_type operator*() const { return Vertex { uint_t(i) }; }

        VertexIterator& operator++()
        {
            advance_to_next();
            return *this;
        }

        VertexIterator operator++(int)
        {
            VertexIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(const VertexIterator& lhs, const VertexIterator& rhs) { return lhs.m_graph == rhs.m_graph && lhs.i == rhs.i; }
        friend bool operator!=(const VertexIterator& lhs, const VertexIterator& rhs) { return !(lhs == rhs); }
    };

    struct EdgeIterator
    {
    private:
        const Graph* m_graph;
        uint_t i;
        boost::dynamic_bitset<>::size_type j;

    private:
        void advance_to_next()
        {
            const auto n = uint_t(m_graph->vertices.size());

            while (i < n)
            {
                const auto& row = m_graph->adjacency_matrix[i];

                if (j == boost::dynamic_bitset<>::npos)
                    j = row.find_next(i);  // find first from i; enforces i < j
                else
                    j = row.find_next(j);  // continue scanning row

                if (j != boost::dynamic_bitset<>::npos)
                    return;  // found edge (i,j)

                ++i;
                j = boost::dynamic_bitset<>::npos;
            }
        }

    public:
        using difference_type = std::ptrdiff_t;
        using value_type = Edge;
        using iterator_category = std::input_iterator_tag;
        using iterator_concept = std::input_iterator_tag;

        EdgeIterator() : m_graph(nullptr), i(0), j(boost::dynamic_bitset<>::npos) {}
        EdgeIterator(const Graph& graph, bool begin) : m_graph(&graph), i(begin ? 0 : uint_t(graph.vertices.size())), j(boost::dynamic_bitset<>::npos)
        {
            if (begin)
                advance_to_next();
        }

        value_type operator*() const { return Edge { Vertex { i }, Vertex { uint_t(j) } }; }

        EdgeIterator& operator++()
        {
            advance_to_next();
            return *this;
        }

        EdgeIterator operator++(int)
        {
            EdgeIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(const EdgeIterator& lhs, const EdgeIterator& rhs) { return lhs.m_graph == rhs.m_graph && lhs.i == rhs.i && lhs.j == rhs.j; }
        friend bool operator!=(const EdgeIterator& lhs, const EdgeIterator& rhs) { return !(lhs == rhs); }
    };

    auto vertices_range() const noexcept { return std::ranges::subrange(VertexIterator(*this, true), VertexIterator(*this, false)); }
    auto edges_range() const noexcept { return std::ranges::subrange(EdgeIterator(*this, true), EdgeIterator(*this, false)); }
};

/// @brief `Workspace` is preallocated memory for a rule.
struct Workspace
{
    std::vector<std::vector<boost::dynamic_bitset<>>> compatible_vertices;  ///< Dimensions K x K x V
    std::vector<boost::dynamic_bitset<>> forbidden_vertices;                /// Dimensions V x V
    boost::dynamic_bitset<> partition_bits;                                 ///< Dimensions K
    std::vector<Vertex> partial_solution;                                   ///< Dimensions K
    uint_t anchor_edge_rank;

    Workspace(size_t k, size_t nv);
};

class DeltaKPKC
{
public:
    DeltaKPKC(const StaticConsistencyGraph& static_graph);

    /// @brief Complete member initialization (for testing)
    DeltaKPKC(ConstGraph const_graph, Graph delta_graph, Graph full_graph, Workspace workspace);

    /// @brief Set delta to current adjacency matrix, recompute current adjacency matrix, transform delta to contain the difference.
    /// @param assignment_sets
    void set_next_assignment_sets(const StaticConsistencyGraph& static_graph, const AssignmentSets& assignment_sets);

    /// @brief Reset should be called before solving for a program.
    void reset()
    {
        m_delta_graph.reset();
        m_full_graph.reset();
        m_iteration = 0;
    }

    /**
     * Parallel interface
     */

    auto delta_vertices_range() const noexcept { return m_delta_graph.vertices_range(); }
    auto delta_edges_range() const noexcept { return m_delta_graph.edges_range(); }
    auto full_vertices_range() const noexcept { return m_full_graph.vertices_range(); }
    auto full_edges_range() const noexcept { return m_full_graph.edges_range(); }

    template<typename Callback>
    void for_each_new_unary_head(Callback&& callback)
    {
        assert(m_const_graph.k == 1);

        for (const auto& vertex : delta_vertices_range())
        {
            m_workspace.partial_solution.clear();
            m_workspace.partial_solution.push_back(vertex);

            callback(m_workspace.partial_solution);
        }
    }

    template<typename Callback>
    void for_each_unary_head(Callback&& callback)
    {
        assert(m_const_graph.k == 1);

        for (const auto& vertex : full_vertices_range())
        {
            m_workspace.partial_solution.clear();
            m_workspace.partial_solution.push_back(vertex);

            callback(m_workspace.partial_solution);
        }
    }

    template<typename Callback>
    void for_each_new_binary_head(Callback&& callback)
    {
        assert(m_const_graph.k == 2);

        for (const auto& edge : delta_edges_range())
        {
            m_workspace.partial_solution.clear();
            m_workspace.partial_solution.push_back(edge.src);
            m_workspace.partial_solution.push_back(edge.dst);

            callback(m_workspace.partial_solution);
        }
    }

    template<typename Callback>
    void for_each_binary_head(Callback&& callback)
    {
        assert(m_const_graph.k == 2);

        for (const auto& edge : full_edges_range())
        {
            m_workspace.partial_solution.clear();
            m_workspace.partial_solution.push_back(edge.src);
            m_workspace.partial_solution.push_back(edge.dst);

            callback(m_workspace.partial_solution);
        }
    }

    template<typename Callback>
    void for_each_k_clique(Callback&& callback)
    {
        const auto k = m_const_graph.k;

        if (k == 0)
        {
            m_workspace.partial_solution.clear();
            callback(m_workspace.partial_solution);
            return;
        }
        else if (k == 1)
        {
            for_each_unary_head(callback);
        }
        else if (k == 2)
        {
            for_each_binary_head(callback);
        }
        else
        {
            seed_without_anchor();

            complete_from_seed<false>(callback, 0);
        }
    }

    template<typename Callback>
    void for_each_new_k_clique(Callback&& callback)
    {
        if (m_iteration == 1)
        {
            for_each_k_clique(callback);
        }
        else
        {
            const auto k = m_const_graph.k;

            if (k == 0)
            {
                return;
            }
            else if (k == 1)
            {
                for_each_new_unary_head(callback);
            }
            else if (k == 2)
            {
                for_each_new_binary_head(callback);
            }
            else
            {
                for (const auto& edge : delta_edges_range())
                {
                    seed_from_anchor(edge);

                    complete_from_seed<true>(callback, 0);
                }
            }
        }
    }

    const ConstGraph& get_const_graph() const noexcept { return m_const_graph; }
    const Graph& get_delta_graph() const noexcept { return m_delta_graph; }
    const Graph& get_full_graph() const noexcept { return m_full_graph; }

private:
    uint_t edge_rank(Edge edge) const { return edge.src.index * m_const_graph.num_vertices + edge.dst.index; }

    bool is_vertex_compatible_with_anchor(Edge edge, Vertex vertex) const
    {
        const auto e1 = Edge(edge.src, vertex);
        const auto e2 = Edge(edge.dst, vertex);

        return (m_full_graph.vertices.test(vertex.index)                                          //
                && m_full_graph.contains(e1)                                                      //                                    //
                && m_full_graph.contains(e2)                                                      //
                && (!m_delta_graph.contains(e1) || edge_rank(e1) > m_workspace.anchor_edge_rank)  //
                && (!m_delta_graph.contains(e2) || edge_rank(e2) > m_workspace.anchor_edge_rank));
    }

    void seed_without_anchor()
    {
        m_workspace.partial_solution.clear();
        m_workspace.partition_bits.reset();
        m_workspace.anchor_edge_rank = std::numeric_limits<uint_t>::max();

        auto& cv_0 = m_workspace.compatible_vertices[0];
        for (uint_t p = 0; p < m_const_graph.k; ++p)
        {
            cv_0[p] = m_const_graph.partition_masks[p];
            cv_0[p] &= m_full_graph.vertices;
        }
    }

    void initialize_forbidden_vertices(uint_t rank)
    {
        for (auto& mask : m_workspace.forbidden_vertices)
            mask.reset();

        for (const auto& other : delta_edges_range())
        {
            const auto other_rank = edge_rank(other);

            if (other_rank == rank)
                continue;

            if (other_rank < rank)
            {
                m_workspace.forbidden_vertices[other.src.index].set(other.dst.index);
                m_workspace.forbidden_vertices[other.dst.index].set(other.src.index);
            }
        }
    }

    void seed_from_anchor(const Edge& edge)
    {
        const uint_t pi = m_const_graph.vertex_to_partition[edge.src.index];
        const uint_t pj = m_const_graph.vertex_to_partition[edge.dst.index];
        assert(pi != pj);

        m_workspace.partial_solution.clear();
        m_workspace.partial_solution.push_back(edge.src);
        m_workspace.partial_solution.push_back(edge.dst);
        m_workspace.anchor_edge_rank = edge_rank(edge);

        initialize_forbidden_vertices(m_workspace.anchor_edge_rank);

        m_workspace.partition_bits.reset();
        m_workspace.partition_bits.set(pi);
        m_workspace.partition_bits.set(pj);

        // Seed candidates for each partition
        auto& cv_0 = m_workspace.compatible_vertices[0];
        for (uint_t p = 0; p < m_const_graph.k; ++p)
        {
            auto& cv_0_p = cv_0[p];
            cv_0_p.reset();

            if (p == pi || p == pj)
                continue;

            // 1. All active vertices in this partition
            cv_0_p = m_const_graph.partition_masks[p];
            // 2. Must be connected to BOTH ends of the anchor in the full graph
            cv_0_p &= m_full_graph.adjacency_matrix[edge.src.index];
            cv_0_p &= m_full_graph.adjacency_matrix[edge.dst.index];
            // 3. Symmetry Breaking: Prune if edge to src/dst is in Delta AND has lower rank
            // Note: Since forbidden_vertices[i] already filters for (rank < anchor),
            // you are effectively doing: next &= ~(is_in_delta && rank_is_lower)
            cv_0_p -= m_workspace.forbidden_vertices[edge.src.index];
            cv_0_p -= m_workspace.forbidden_vertices[edge.dst.index];
        }
    }

    uint_t choose_best_partition(size_t depth) const
    {
        const uint_t k = m_const_graph.k;
        const auto& cv_d = m_workspace.compatible_vertices[depth];
        const auto& partition_bits = m_workspace.partition_bits;

        uint_t best_partition = std::numeric_limits<uint_t>::max();
        uint_t best_set_bits = std::numeric_limits<uint_t>::max();

        for (uint_t p = 0; p < k; ++p)
        {
            if (partition_bits.test(p))
                continue;

            auto num_set_bits = cv_d[p].count();
            if (num_set_bits < best_set_bits)
            {
                best_set_bits = num_set_bits;
                best_partition = p;
            }
        }
        return best_partition;
    }

    /// @brief Restrict candidates in each remaining partition to those with an edge connecting to the last chosen vertex `src`.
    /// @tparam Delta
    /// @param src
    /// @param depth
    template<bool Delta>
    void update_compatible_adjacent_vertices_at_next_depth(Vertex src, size_t depth)
    {
        const uint_t k = m_const_graph.k;

        const auto& cv_curr = m_workspace.compatible_vertices[depth];
        auto& cv_next = m_workspace.compatible_vertices[depth + 1];
        const auto& partition_bits = m_workspace.partition_bits;

        // Get the adjacency row for the new vertex once
        const auto& full_adj = m_full_graph.adjacency_matrix[src.index];
        const auto& forbidden_mask = m_workspace.forbidden_vertices[src.index];

        for (uint_t p = 0; p < k; ++p)
        {
            // Skip partitions already represented in the partial solution
            if (partition_bits.test(p))
                continue;

            auto& cv_next_p = cv_next[p];

            // FUSED OPERATION:
            // 1. Assignment (=) performs the copy.
            // 2. The bitwise logic (&) performs the pruning.
            // This happens in one single movement of data into the CPU registers.
            cv_next[p] = cv_curr[p] & full_adj;

            // 2. Delta-Rank Pruning (The "Lowest Rank" Rule)
            if constexpr (Delta)
            {
                // Subtract vertices that form a delta-edge with 'src'
                // having a rank lower than the anchor.
                cv_next_p -= forbidden_mask;
            }
        }
    }

    uint_t num_possible_additions_at_next_depth(size_t depth)
    {
        const uint_t k = m_const_graph.k;
        const auto& cv_d_next = m_workspace.compatible_vertices[depth + 1];
        const auto& partition_bits = m_workspace.partition_bits;

        uint_t possible_additions = 0;
        for (uint_t partition = 0; partition < k; ++partition)
            if (!partition_bits.test(partition) && cv_d_next[partition].any())
                ++possible_additions;

        return possible_additions;
    }

    template<bool Delta, class Callback>
    void complete_from_seed(Callback&& callback, size_t depth)
    {
        assert(depth < m_const_graph.k);

        const uint_t p = choose_best_partition(depth);
        if (p == std::numeric_limits<uint_t>::max())
            return;  // dead branch: no unused partition has candidates

        const uint_t k = m_const_graph.k;
        auto& cv_d_p = m_workspace.compatible_vertices[depth][p];
        auto& partition_bits = m_workspace.partition_bits;
        auto& partial_solution = m_workspace.partial_solution;

        // Iterate through compatible vertices in the best partition
        for (auto v = cv_d_p.find_first(); v != boost::dynamic_bitset<>::npos; v = cv_d_p.find_next(v))
        {
            const auto vertex = Vertex(v);

            partial_solution.push_back(vertex);

            if (partial_solution.size() == k)
            {
                callback(partial_solution);
            }
            else
            {
                update_compatible_adjacent_vertices_at_next_depth<Delta>(vertex, depth);

                partition_bits.set(p);

                if ((partial_solution.size() + num_possible_additions_at_next_depth(depth)) == k)
                    complete_from_seed<Delta>(callback, depth + 1);

                partition_bits.reset(p);
            }

            partial_solution.pop_back();
        }
    }

private:
    ConstGraph m_const_graph;
    Graph m_delta_graph;
    Graph m_full_graph;
    Workspace m_workspace;
    size_t m_iteration;
};

}

#endif
