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

#ifndef TYR_GRAPHS_BGL_ALGORITHMS_HPP_
#define TYR_GRAPHS_BGL_ALGORITHMS_HPP_

#include "tyr/common/config.hpp"
#include "tyr/graphs/concepts.hpp"

#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <limits>

namespace tyr::graphs
{
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// boost::dijkstra_shortest_path
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

template<typename Graph, class SourceInputIter>
    requires graphs::ContiguousIndexingMode<Graph> && graphs::VertexListGraph<Graph> && graphs::IncidenceGraph<Graph>
auto dijkstra_shortest_paths(const Graph& g, const std::vector<float_t>& w, SourceInputIter s_begin, SourceInputIter s_end)
{
    // TODO: Vertex and IncidenceGraph dont provide num_edges.
    assert(w.size() == g.num_edges());

    using vertex_descriptor_type = typename boost::graph_traits<Graph>::vertex_descriptor;
    // using edge_descriptor_type = typename boost::graph_traits<Graph>::edge_descriptor;

    auto p = std::vector<vertex_descriptor_type>(num_vertices(g));
    auto index_map = boost::identity_property_map();
    auto predecessor_map = boost::make_iterator_property_map(p.begin(), index_map);
    auto d = std::vector<float_t>(num_vertices(g));
    auto distance_map = boost::make_iterator_property_map(d.begin(), index_map);
    auto weight_map = boost::make_iterator_property_map(w.begin(), boost::identity_property_map());
    auto compare = std::less<float_t>();
    auto combine = std::plus<float_t>();
    auto inf = std::numeric_limits<float_t>::infinity();
    auto zero = float_t();

    // multiple source shortest path version.
    boost::dijkstra_shortest_paths(g,  //
                                   s_begin,
                                   s_end,
                                   predecessor_map,
                                   distance_map,
                                   weight_map,
                                   index_map,
                                   compare,
                                   combine,
                                   inf,
                                   zero,
                                   boost::default_dijkstra_visitor());

    return std::make_tuple(p, d);
};
}

#endif
