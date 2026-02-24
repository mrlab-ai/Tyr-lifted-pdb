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

#include "tyr/datalog/consistency_graph.hpp"
#include "tyr/datalog/delta_kpkc.hpp"
#include "tyr/formalism/datalog/expression_arity.hpp"

namespace f = tyr::formalism;
namespace fd = tyr::formalism::datalog;

namespace tyr::datalog::kpkc
{

[[maybe_unused]] static bool verify_vertex_partitions(const std::vector<std::vector<uint_t>>& vertex_partitions)
{
    uint_t i = 0;
    for (const auto& partition : vertex_partitions)
        for (const auto& v : partition)
            if (v != i++)
                return false;
    return true;
}

[[maybe_unused]] static bool verify_vertex_to_partition(size_t nv, size_t k, const std::vector<uint_t>& vertex_to_partition)
{
    if (vertex_to_partition.size() != nv)
        return false;

    for (uint_t v = 0; v < nv; ++v)
        if (vertex_to_partition[v] >= k)
            return false;

    return true;
}

GraphLayout::GraphLayout(size_t nv, const std::vector<std::vector<uint_t>>& vertex_partitions_) :
    nv(nv),
    k(vertex_partitions_.size()),
    vertex_partitions(vertex_partitions_),
    vertex_to_partition(),
    vertex_to_bit(),
    info()
{
    assert(verify_vertex_partitions(vertex_partitions_));

    vertex_to_partition.resize(nv);
    vertex_to_bit.resize(nv);
    info.infos.reserve(k);

    uint_t block_offset = uint_t(0);
    uint_t bit_offset = uint_t(0);

    for (size_t p = 0; p < k; ++p)
    {
        const auto& partition = vertex_partitions[p];

        const auto partition_size = static_cast<uint_t>(partition.size());
        const auto partition_blocks = static_cast<uint_t>(BitsetSpan<uint64_t>::num_blocks(partition_size));
        info.infos.push_back(GraphLayout::BitsetInfo { bit_offset, partition_size, block_offset, partition_blocks });
        block_offset += partition_blocks;

        uint_t bit = 0;
        for (const auto& v : partition)
        {
            vertex_to_bit[v] = bit++;
            vertex_to_partition[v] = p;

            ++bit_offset;
        }
    }

    info.num_blocks = block_offset;

    assert(verify_vertex_to_partition(nv, k, vertex_to_partition));
}

Workspace::Workspace(const GraphLayout& graph) :
    compatible_vertices_data(graph.k * graph.info.num_blocks, 0),
    compatible_vertices_span(compatible_vertices_data.data(), std::array<size_t, 2> { graph.k, graph.info.num_blocks }),
    partition_bits(graph.k, false),
    partial_solution(graph.k),
    partial_solution_size(0)
{
}

/**
 * VariableDependencyGraph
 */

template<f::FactKind T>
static void insert_literal(View<Index<fd::Literal<T>>, fd::Repository> literal,
                           uint_t k,
                           boost::dynamic_bitset<>& positive_dependencies,
                           boost::dynamic_bitset<>& negative_dependencies)
{
    const auto parameters_set = collect_parameters(literal);
    auto parameters = std::vector<f::ParameterIndex>(parameters_set.begin(), parameters_set.end());
    std::sort(parameters.begin(), parameters.end());

    for (uint_t i = 0; i < parameters.size(); ++i)
    {
        const auto pi = uint_t(parameters[i]);

        for (uint_t j = i + 1; j < parameters.size(); ++j)
        {
            const auto pj = uint_t(parameters[j]);

            const auto i1 = VariableDependencyGraph::get_index(pi, pj, k);
            const auto i2 = VariableDependencyGraph::get_index(pj, pi, k);

            if (literal.get_polarity())
            {
                positive_dependencies.set(i1);
                positive_dependencies.set(i2);
            }
            else
            {
                negative_dependencies.set(i1);
                negative_dependencies.set(i2);
            }
        }
    }
}

static void insert_numeric_constraint(View<Data<fd::BooleanOperator<Data<fd::FunctionExpression>>>, fd::Repository> numeric_constraint,
                                      uint_t k,
                                      boost::dynamic_bitset<>& positive_dependencies)
{
    const auto parameters_set = collect_parameters(numeric_constraint);
    auto parameters = std::vector<f::ParameterIndex>(parameters_set.begin(), parameters_set.end());
    std::sort(parameters.begin(), parameters.end());

    for (uint_t i = 0; i < parameters.size(); ++i)
    {
        const auto pi = uint_t(parameters[i]);

        for (uint_t j = i + 1; j < parameters.size(); ++j)
        {
            const auto pj = uint_t(parameters[j]);

            const auto i1 = VariableDependencyGraph::get_index(pi, pj, k);
            const auto i2 = VariableDependencyGraph::get_index(pj, pi, k);

            positive_dependencies.set(i1);
            positive_dependencies.set(i2);
        }
    }
}

VariableDependencyGraph::VariableDependencyGraph(View<Index<formalism::datalog::ConjunctiveCondition>, formalism::datalog::Repository> condition) :
    m_k(condition.get_arity()),
    m_static_positive_dependencies(m_k * m_k),
    m_static_negative_dependencies(m_k * m_k),
    m_fluent_positive_dependencies(m_k * m_k),
    m_fluent_negative_dependencies(m_k * m_k)
{
    for (const auto literal : condition.get_literals<f::StaticTag>())
        insert_literal(literal, m_k, m_static_positive_dependencies, m_static_negative_dependencies);

    for (const auto literal : condition.get_literals<f::FluentTag>())
        insert_literal(literal, m_k, m_fluent_positive_dependencies, m_fluent_negative_dependencies);

    for (const auto numeric_constraint : condition.get_numeric_constraints())
        insert_numeric_constraint(numeric_constraint, m_k, m_fluent_positive_dependencies);
}

}
