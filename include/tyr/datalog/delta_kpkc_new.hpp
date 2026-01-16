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

#ifndef TYR_DATALOG_KPKC_NEW_HPP_
#define TYR_DATALOG_KPKC_NEW_HPP_

#include "tyr/common/formatter.hpp"
#include "tyr/datalog/declarations.hpp"

#include <boost/dynamic_bitset.hpp>
#include <cstddef>
#include <iostream>
#include <vector>

namespace tyr::datalog::delta_kpkc2
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

struct VertexSet
{
    boost::dynamic_bitset<> partition_bits;
    size_t partition_count;

    boost::dynamic_bitset<> vertex_bits;
    size_t vertex_count;
};

struct ConstGraph
{
    /// Meta
    size_t num_vertices;
    size_t k;
    /// Vertex partitioning
    std::vector<std::vector<Vertex>> partitions;  ///< Dimensions K x V
    std::vector<uint_t> vertex_to_partition;
    /// Vertex sets
    VertexSet head;
    VertexSet non_head;
    VertexSet full;
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
        const boost::dynamic_bitset<>* m_mask;
        boost::dynamic_bitset<>::size_type i;

    private:
        void skip_invalid()
        {
            while (i != boost::dynamic_bitset<>::npos && m_mask && !m_mask->test(i))
                i = m_graph->vertices.find_next(i);
        }

    public:
        using difference_type = std::ptrdiff_t;
        using value_type = Vertex;
        using iterator_category = std::input_iterator_tag;
        using iterator_concept = std::input_iterator_tag;

        VertexIterator() : m_graph(nullptr), m_mask(nullptr), i(boost::dynamic_bitset<>::npos) {}
        VertexIterator(const Graph& graph, const boost::dynamic_bitset<>& mask, bool begin) :
            m_graph(&graph),
            m_mask(&mask),
            i(begin ? graph.vertices.find_first() : boost::dynamic_bitset<>::npos)
        {
            assert(mask.size() == graph.vertices.size());
            skip_invalid();
        }

        value_type operator*() const { return Vertex { uint_t(i) }; }

        VertexIterator& operator++()
        {
            if (i != boost::dynamic_bitset<>::npos)
            {
                i = m_graph->vertices.find_next(i);
                skip_invalid();
            }
            return *this;
        }

        VertexIterator operator++(int)
        {
            VertexIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(const VertexIterator& lhs, const VertexIterator& rhs)
        {
            return lhs.m_graph == rhs.m_graph && lhs.m_mask == rhs.m_mask && lhs.i == rhs.i;
        }
        friend bool operator!=(const VertexIterator& lhs, const VertexIterator& rhs) { return !(lhs == rhs); }
    };

    struct EdgeIterator
    {
    private:
        const Graph* m_graph;
        const boost::dynamic_bitset<>* m_src_mask;
        const boost::dynamic_bitset<>* m_dst_mask;
        uint_t i;
        boost::dynamic_bitset<>::size_type j;

    private:
        bool src_ok(uint_t u) const { return m_graph->vertices.test(u) && (!m_src_mask || m_src_mask->test(u)); }

        bool dst_ok(uint_t v) const { return m_graph->vertices.test(v) && (!m_dst_mask || m_dst_mask->test(v)); }

        void advance_to_next_valid()
        {
            const auto n = uint_t(m_graph->vertices.size());

            while (i < n)
            {
                if (!src_ok(i))
                {
                    ++i;
                    j = boost::dynamic_bitset<>::npos;
                    continue;
                }

                const auto& row = m_graph->adjacency_matrix[i];

                j = (j == boost::dynamic_bitset<>::npos) ? row.find_next(i) : row.find_next(j);

                while (j != boost::dynamic_bitset<>::npos && !dst_ok(uint_t(j)))
                    j = row.find_next(j);

                if (j != boost::dynamic_bitset<>::npos)
                    return;  // found valid (i,j)

                ++i;
                j = boost::dynamic_bitset<>::npos;
            }
            j = boost::dynamic_bitset<>::npos;
        }

    public:
        using difference_type = std::ptrdiff_t;
        using value_type = Edge;
        using iterator_category = std::input_iterator_tag;
        using iterator_concept = std::input_iterator_tag;

        EdgeIterator() : m_graph(nullptr), m_src_mask(nullptr), m_dst_mask(nullptr), i(0), j(boost::dynamic_bitset<>::npos) {}
        EdgeIterator(const Graph& graph, const boost::dynamic_bitset<>& src_mask, const boost::dynamic_bitset<>& dst_mask, bool begin) :
            m_graph(&graph),
            m_src_mask(&src_mask),
            m_dst_mask(&dst_mask),
            i(begin ? 0 : uint_t(graph.vertices.size())),
            j(boost::dynamic_bitset<>::npos)
        {
            assert(src_mask.size() == graph.vertices.size());
            assert(dst_mask.size() == graph.vertices.size());

            if (begin)
                advance_to_next_valid();
        }

        value_type operator*() const { return Edge { Vertex { i }, Vertex { uint_t(j) } }; }

        EdgeIterator& operator++()
        {
            advance_to_next_valid();
            return *this;
        }

        EdgeIterator operator++(int)
        {
            EdgeIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(const EdgeIterator& lhs, const EdgeIterator& rhs)
        {
            return lhs.m_graph == rhs.m_graph && lhs.m_src_mask == rhs.m_src_mask && lhs.m_dst_mask == rhs.m_dst_mask && lhs.i == rhs.i && lhs.j == rhs.j;
        }
        friend bool operator!=(const EdgeIterator& lhs, const EdgeIterator& rhs) { return !(lhs == rhs); }
    };

    auto vertices_range(const boost::dynamic_bitset<>& mask) const noexcept
    {
        return std::ranges::subrange(VertexIterator(*this, mask, true), VertexIterator(*this, mask, false));
    }
    auto edges_range(const boost::dynamic_bitset<>& src_mask, const boost::dynamic_bitset<>& dst_mask) const noexcept
    {
        return std::ranges::subrange(EdgeIterator(*this, src_mask, dst_mask, true), EdgeIterator(*this, src_mask, dst_mask, false));
    }
};

/// @brief `Workspace` is preallocated memory for a rule.
struct Workspace
{
    std::vector<std::vector<boost::dynamic_bitset<>>> compatible_vertices;  ///< Dimensions K x K x V
    boost::dynamic_bitset<> partition_bits;                                 ///< Dimensions K
    std::vector<Vertex> partial_solution;                                   ///< Dimensions K
    uint_t anchor_edge_rank;
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
    }

    // Case 1: delta edge (u,v) with u,v in head => anchor (u,v), fill head clique and call head_callback, then fill full clique and call full_callback
    // Case 2: no delta edge (u,v) with u,v in head:
    // Case 2.1: delta edge (u',v') with u',v' in rule => anchor (u', v') fill full clique and call full_callback
    // Case 2.2: no delta edge (u', v') with u',v' in rule => skip

    /**
     * Parallel interface
     */

    auto delta_vertices_range(const boost::dynamic_bitset<>& mask) const noexcept { return m_delta_graph.vertices_range(mask); }
    auto delta_edges_range(const boost::dynamic_bitset<>& src_mask, const boost::dynamic_bitset<>& dst_mask) const noexcept
    {
        return m_delta_graph.edges_range(src_mask, dst_mask);
    }
    auto full_vertices_range(const boost::dynamic_bitset<>& mask) const noexcept { return m_full_graph.vertices_range(mask); }
    auto full_edges_range(const boost::dynamic_bitset<>& src_mask, const boost::dynamic_bitset<>& dst_mask) const noexcept
    {
        return m_full_graph.edges_range(src_mask, dst_mask);
    }

    template<typename Callback>
    void for_each_new_unary_head(Callback&& callback)
    {
        assert(m_const_graph.head.partition_count == 1);

        for (const auto& vertex : delta_vertices_range(m_const_graph.head.vertex_bits))
        {
            m_workspace.partial_solution.clear();
            m_workspace.partial_solution.push_back(vertex);

            callback(m_workspace.partial_solution);
        }
    }

    template<typename Callback>
    void for_each_unary_head(Callback&& callback)
    {
        assert(m_const_graph.head.partition_count == 1);

        for (const auto& vertex : full_vertices_range(m_const_graph.head.vertex_bits))
        {
            m_workspace.partial_solution.clear();
            m_workspace.partial_solution.push_back(vertex);

            callback(m_workspace.partial_solution);
        }
    }

    template<typename Callback>
    void for_each_new_binary_head(Callback&& callback)
    {
        assert(m_const_graph.head.partition_count == 2);

        for (const auto& edge : delta_edges_range(m_const_graph.head.vertex_bits, m_const_graph.head.vertex_bits))
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
        assert(m_const_graph.head.partition_count == 2);

        for (const auto& edge : full_edges_range(m_const_graph.head.vertex_bits, m_const_graph.head.vertex_bits))
        {
            m_workspace.partial_solution.clear();
            m_workspace.partial_solution.push_back(edge.src);
            m_workspace.partial_solution.push_back(edge.dst);

            callback(m_workspace.partial_solution);
        }
    }

    /// @brief Enumerate all new head cliques.
    /// @tparam Callback
    /// @param callback
    template<typename Callback>
    void for_each_new_head_clique(Callback&& callback)
    {
        const auto h = m_const_graph.head.partition_count;

        if (h == 0)
        {
            m_workspace.partial_solution.clear();
            callback(m_workspace.partial_solution);
        }
        else if (h == 1)
        {
            for_each_new_unary_head(callback);
        }
        else if (h == 2)
        {
            for_each_new_binary_head(callback);
        }
        else
        {
            for (const auto& edge : delta_edges_range(m_const_graph.head.vertex_bits, m_const_graph.head.vertex_bits))
            {
                // seed_from_anchor(Edge(src, Vertex(j)));

                // complete_head_from_seed(callback, 0);
            }
        }
    }

    /// @brief Exhaustively enumerate all head cliques.
    /// @tparam Callback
    /// @param callback
    template<typename Callback>
    void for_each_head_clique(Callback&& callback)
    {
        const auto h = m_const_graph.head.partition_count;

        if (h == 0)
        {
            m_workspace.partial_solution.clear();
            callback(m_workspace.partial_solution);
        }
        else if (h == 1)
        {
            for_each_unary_head(callback);
        }
        else if (h == 2)
        {
            for_each_binary_head(callback);
        }
        else
        {
            for (const auto& edge : full_edges_range(m_const_graph.head.vertex_bits, m_const_graph.head.vertex_bits))
            {
                seed_from_anchor(edge);

                complete_from_seed<true>(callback, m_const_graph.head, 0);
            }
        }
    }

    /// @brief Enumerate all new continuations of the fixed head clique.
    /// @tparam Callback
    /// @param callback
    /// @param require_delta
    template<typename Callback>
    void for_each_new_rule_clique(Callback&& callback)
    {
    }

    /// @brief Exhaustively enumerate all continuations of the fixed head clique.
    /// @tparam Callback
    /// @param callback
    template<typename Callback>
    void for_each_rule_clique(Callback&& callback)
    {
    }

    const ConstGraph& get_const_graph() const noexcept { return m_const_graph; }
    const Graph& get_delta_graph() const noexcept { return m_delta_graph; }
    const Graph& get_full_graph() const noexcept { return m_full_graph; }

private:
    void seed_from_anchor(const Edge& edge)
    {
        const uint_t pi = m_const_graph.vertex_to_partition[edge.src.index];
        const uint_t pj = m_const_graph.vertex_to_partition[edge.dst.index];
        assert(pi != pj);

        m_workspace.partial_solution.clear();
        m_workspace.partial_solution.push_back(edge.src);
        m_workspace.partial_solution.push_back(edge.dst);
        m_workspace.anchor_edge_rank = edge_rank(edge);

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

            const auto& part = m_const_graph.partitions[p];
            for (uint_t bit = 0; bit < part.size(); ++bit)
                if (is_vertex_compatible_with_anchor(edge, Vertex { part[bit] }))
                    cv_0_p.set(bit);
        }
    }

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

    uint_t choose_best_partition(size_t depth, const VertexSet& vertices) const
    {
        const auto& cv_d = m_workspace.compatible_vertices[depth];
        const auto& partition_bits = m_workspace.partition_bits;

        uint_t best_partition = std::numeric_limits<uint_t>::max();
        uint_t best_set_bits = std::numeric_limits<uint_t>::max();

        for (auto p = vertices.partition_bits.find_first(); p != boost::dynamic_bitset<>::npos; p = vertices.partition_bits.find_next(p))
        {
            if (partition_bits.test(p))
                continue;  /// Skip partition p if p was previously chosen

            auto num_set_bits = cv_d[p].count();
            if (num_set_bits < best_set_bits)
            {
                best_set_bits = num_set_bits;
                best_partition = uint_t(p);
            }
        }
        return best_partition;
    }

    void copy_current_compatible_vertices_to_next_depth(size_t depth)
    {
        const uint_t k = m_const_graph.k;
        auto& cv_d = m_workspace.compatible_vertices[depth];
        auto& next = m_workspace.compatible_vertices[depth + 1];

        for (uint_t p = 0; p < k; ++p)
            next[p] = cv_d[p];
    }

    template<bool Delta>
    void update_compatible_adjacent_vertices_at_next_depth(Vertex src, size_t depth)
    {
        const uint_t k = m_const_graph.k;
        auto& cv_d_next = m_workspace.compatible_vertices[depth + 1];
        const auto& partition_bits = m_workspace.partition_bits;

        // Offset trick assumes that vertices in lower partition have lower indices.
        uint_t offset = 0;
        for (uint_t partition = 0; partition < k; ++partition)
        {
            auto& cv_d_next_p = cv_d_next[partition];
            auto partition_size = cv_d_next_p.size();
            if (!partition_bits.test(partition))
            {
                for (uint_t bit = 0; bit < partition_size; ++bit)
                {
                    const auto dst = Vertex(bit + offset);
                    const auto edge = Edge(src, dst);

                    cv_d_next_p[bit] &= m_full_graph.contains(edge);

                    // monotone delta-rank pruning
                    if constexpr (Delta)
                        if (cv_d_next_p.test(bit) && m_delta_graph.contains(edge) && edge_rank(edge) < m_workspace.anchor_edge_rank)
                            cv_d_next_p.reset(bit);
                }
            }
            offset += partition_size;
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
    void complete_from_seed(Callback&& callback, const VertexSet& vertices, size_t depth)
    {
        assert(depth < m_const_graph.k);

        const uint_t p = choose_best_partition(depth, vertices);
        if (p == std::numeric_limits<uint_t>::max())
            return;  // dead branch: no unused partition has candidates

        const uint_t k = m_const_graph.k;
        auto& cv_d_p = m_workspace.compatible_vertices[depth][p];
        auto& partition_bits = m_workspace.partition_bits;
        auto& partial_solution = m_workspace.partial_solution;

        // Iterate through compatible vertices in the best partition
        for (auto bit = cv_d_p.find_first(); bit != boost::dynamic_bitset<>::npos; bit = cv_d_p.find_next(bit))
        {
            cv_d_p.reset(bit);

            const auto vertex = m_const_graph.partitions[p][bit];

            partial_solution.push_back(vertex);

            if (partial_solution.size() == vertices.partition_count)
            {
                callback(partial_solution);
            }
            else
            {
                copy_current_compatible_vertices_to_next_depth(depth);

                update_compatible_adjacent_vertices_at_next_depth<Delta>(vertex, depth);

                partition_bits.set(p);

                if ((partial_solution.size() + num_possible_additions_at_next_depth(depth)) == vertices.partition_count)
                    complete_from_seed<Delta>(callback, vertices, depth + 1);

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
};

}

#endif
