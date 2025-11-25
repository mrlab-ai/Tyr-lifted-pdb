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

#ifndef TYR_GROUNDER_KPKC_UTILS_HPP_
#define TYR_GROUNDER_KPKC_UTILS_HPP_

#include "tyr/formalism/declarations.hpp"
#include "tyr/grounder/declarations.hpp"
#include "tyr/grounder/kpkc_data.hpp"

#include <boost/dynamic_bitset.hpp>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <tyr/common/config.hpp>
#include <vector>

namespace tyr::grounder::kpkc
{

/// @brief Helper to allocate a DenseKPartiteGraph from a given StaticConsistencyGraph.
template<formalism::IsContext C>
inline DenseKPartiteGraph allocate_dense_graph(const StaticConsistencyGraph<C>& sparse_graph)
{
    auto graph = DenseKPartiteGraph();
    return graph;
}

/// @brief Helper to allocate a Workspace from a given StaticConsistencyGraph.
template<formalism::IsContext C>
inline Workspace allocate_workspace(const StaticConsistencyGraph<C>& sparse_graph)
{
    auto workspace = Workspace();
    return workspace;
}

}

#endif