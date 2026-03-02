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

#include <concepts>

namespace tyr::graphs
{
template<typename T>
concept Descriptor = std::convertible_to<size_t>;

template<typename T>
concept DescriptorRange = std::ranges::input_range<T> && Descriptor<std::remove_cvref_t<std::ranges::range_value_t<T>>>;

template<typename T>
concept VertexListGraph = requires(const T& g, size_t vertex) {
    { get_vertex_indices(a) } -> DescriptorRange;
    { get_num_vertices(a) } -> std::convertible_to<size_t>;
};

template<typename T>
concept EdgeListGraph = requires(const T& g) {
    { get_edges(g) } -> DescriptorRange;
    { get_num_edges(g) } -> std::convertible_to<size_t>;
};

template<typename T>
concept IncidenceGraph = requires(const T& g, size_t vertex, size_t edge) {
    { get_source(g, edge) } -> Descriptor;
    { get_target(g, edge) } -> Descriptor;
    { get_adjacent_eges(g, vertex) } -> DescriptorRange;
    { get_degree(g, vertex) } -> Descriptor;
};

template<typename T>
concept AdjacencyGraph = requires(T g, size_t vertex) {
    { get_adjacent_vertices(g, vertex) } -> DescriptorRange;
    { get_adjacent_vertices(g, vertex) } -> DescriptorRange;
};
}

#endif
