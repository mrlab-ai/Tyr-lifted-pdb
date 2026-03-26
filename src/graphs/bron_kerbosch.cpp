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

#include "tyr/graphs/bron_kerbosch.hpp"

#include <boost/dynamic_bitset.hpp>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace tyr::graphs::bron_kerbosch
{
namespace
{

void validate_graph(const Graph& graph)
{
    const auto n = graph.matrix.size();
    for (const auto& row : graph.matrix)
    {
        if (row.size() != n)
        {
            throw std::invalid_argument("bron_kerbosch::Graph must be square");
        }
    }
}

uint_t choose_pivot(const Graph& graph, const boost::dynamic_bitset<>& candidates, const boost::dynamic_bitset<>& excluded)
{
    const auto union_set = candidates | excluded;

    const auto first = union_set.find_first();
    if (first == boost::dynamic_bitset<>::npos)
    {
        throw std::logic_error("choose_pivot called with empty candidates ∪ excluded");
    }

    auto best_pivot = static_cast<uint_t>(first);
    auto best_score = (candidates & graph.matrix[first]).count();

    for (auto u = union_set.find_next(first); u != boost::dynamic_bitset<>::npos; u = union_set.find_next(u))
    {
        const auto score = (candidates & graph.matrix[u]).count();
        if (score > best_score)
        {
            best_pivot = static_cast<uint_t>(u);
            best_score = score;
        }
    }

    return best_pivot;
}

void bron_kerbosch_pivot(const Graph& graph,
                         std::vector<uint_t>& current_clique,
                         boost::dynamic_bitset<>& candidates,
                         boost::dynamic_bitset<>& excluded,
                         std::vector<std::vector<uint_t>>& maximal_cliques)
{
    if (candidates.none() && excluded.none())
    {
        maximal_cliques.push_back(current_clique);
        return;
    }

    const auto pivot = choose_pivot(graph, candidates, excluded);

    // Iterate over P \ N(u)
    auto extension = candidates & ~graph.matrix[pivot];

    for (auto v = extension.find_first(); v != boost::dynamic_bitset<>::npos; v = extension.find_next(v))
    {
        current_clique.push_back(static_cast<uint_t>(v));

        auto new_candidates = candidates & graph.matrix[v];
        auto new_excluded = excluded & graph.matrix[v];

        bron_kerbosch_pivot(graph, current_clique, new_candidates, new_excluded, maximal_cliques);

        current_clique.pop_back();

        candidates.reset(v);
        excluded.set(v);
    }
}

}  // namespace

Graph::Graph() : matrix() {}

Graph::Graph(size_t nv) : matrix()
{
    matrix.resize(nv);
    for (auto& bitset : matrix)
        bitset.resize(nv);
}

std::vector<std::vector<uint_t>> compute_maximal_cliques(const Graph& graph)
{
    validate_graph(graph);

    const auto n = graph.matrix.size();

    std::vector<std::vector<uint_t>> maximal_cliques;
    std::vector<uint_t> current_clique;

    if (n == 0)
    {
        maximal_cliques.emplace_back();
        return maximal_cliques;
    }

    boost::dynamic_bitset<> candidates(n);
    candidates.set();

    boost::dynamic_bitset<> excluded(n);

    bron_kerbosch_pivot(graph, current_clique, candidates, excluded, maximal_cliques);
    return maximal_cliques;
}

}  // namespace tyr::graphs::bron_kerbosch