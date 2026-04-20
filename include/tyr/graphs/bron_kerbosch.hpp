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

#ifndef TYR_GRAPHS_BRON_KERBOSCH_HPP_
#define TYR_GRAPHS_BRON_KERBOSCH_HPP_

#include "tyr/common/config.hpp"

#include <boost/dynamic_bitset.hpp>
#include <vector>

namespace tyr::graphs::bron_kerbosch
{

struct Graph
{
    std::vector<boost::dynamic_bitset<>> matrix;

    Graph();
    explicit Graph(size_t nv);
};

std::vector<std::vector<uint_t>> compute_maximal_cliques(const Graph& graph);

}

#endif
