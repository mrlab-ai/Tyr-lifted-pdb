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

#ifndef TYR_GRAPHS_CONCEPTS_HPP_
#define TYR_GRAPHS_CONCEPTS_HPP_

#include "tyr/common/config.hpp"

#include <concepts>
#include <cstddef>

namespace tyr::graphs
{
template<typename T>
concept Descriptor = std::convertible_to<T, uint_t>;

template<typename T>
concept DescriptorRange = std::ranges::input_range<T> && Descriptor<std::remove_cvref_t<std::ranges::range_value_t<T>>>;

template<typename T>
concept VertexListGraph = requires(const T& g) {
    { vertices(g) };
    { num_vertices(g) } -> std::convertible_to<size_t>;
};

template<typename T>
concept EdgeListGraph = requires(const T& g, uint_t e) {
    { edges(g) };
    { num_edges(g) } -> std::convertible_to<size_t>;
    { source(e, g) } -> Descriptor;
    { target(e, g) } -> Descriptor;
};

template<typename T>
concept IncidenceGraph = requires(const T& g, uint_t v, uint_t e) {
    { out_edges(v, g) };
    { source(e, g) } -> Descriptor;
    { target(e, g) } -> Descriptor;
    { out_degree(v, g) } -> Descriptor;
};

template<typename T>
concept AdjacencyGraph = requires(T g, uint_t v) {
    { adjacent_vertices(v, g) } -> DescriptorRange;
};

struct ContiguousIndexingTag
{
};

template<typename T>
concept ContiguousIndexingMode = requires(const T& g) { requires std::same_as<typename T::IndexingMode, ContiguousIndexingTag>; };
}

#endif
