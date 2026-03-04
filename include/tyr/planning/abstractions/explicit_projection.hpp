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

#ifndef TYR_PLANNING_ABSTRACTIONS_EXPLICIT_PROJECTION_HPP_
#define TYR_PLANNING_ABSTRACTIONS_EXPLICIT_PROJECTION_HPP_

#include "tyr/common/config.hpp"
#include "tyr/common/declarations.hpp"
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/formalism/planning/views.hpp"
#include "tyr/graphs/concepts.hpp"
#include "tyr/planning/state.hpp"

#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/graph_concepts.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <ranges>

namespace tyr::planning
{

struct Transition
{
    View<Index<formalism::planning::GroundAction>, formalism::planning::Repository> label;
    uint_t src;
    uint_t dst;
};

using TransitionList = std::vector<Transition>;

template<typename Task>
class ExplicitProjection
{
private:
    std::vector<State<Task>> m_vertices;
    TransitionList m_transitions;
    std::vector<std::vector<uint_t>> m_adj_lists;

public:
    using IndexingMode = graphs::ContiguousIndexingTag;

    ExplicitProjection(std::vector<State<Task>> vertices, TransitionList transitions, std::vector<std::vector<uint_t>> adj_lists) :
        m_vertices(std::move(vertices)),
        m_transitions(std::move(transitions)),
        m_adj_lists(std::move(adj_lists))
    {
    }

    auto num_vertices() const noexcept { return m_vertices.size(); }
    auto num_edges() const noexcept { return m_transitions.size(); }
    const auto& vertices() const noexcept { return m_vertices; }
    const auto& transitions() const noexcept { return m_transitions; }
    const auto& adj_lists() const noexcept { return m_adj_lists; }
};

template<typename Task>
class BackwardExplicitProjection
{
private:
    const ExplicitProjection<Task>& m_g;

    std::vector<std::vector<uint_t>> m_adj_lists;

public:
    using IndexingMode = graphs::ContiguousIndexingTag;

    explicit BackwardExplicitProjection(const ExplicitProjection<Task>& g) : m_g(g), m_adj_lists(g.num_vertices())
    {
        for (uint_t t = 0; t < g.num_edges(); ++t)
            m_adj_lists[g.transitions()[t].dst].push_back(t);
    }

    auto num_vertices() const noexcept { return m_g.num_vertices(); }
    auto num_edges() const noexcept { return m_g.num_edges(); }
    const auto& vertices() const noexcept { return m_g.vertices(); }
    const auto& transitions() const noexcept { return m_g.transitions(); }
    const auto& adj_lists() const noexcept { return m_adj_lists; }
    const auto& g() const noexcept { return m_g; }
};

/**
 * VertexListGraph
 */

template<typename Task>
auto num_vertices(const ExplicitProjection<Task>& g)
{
    return g.num_vertices();
}

template<typename Task>
auto vertices(const ExplicitProjection<Task>& g)
{
    using It = boost::counting_iterator<uint_t>;
    return std::make_pair(It { 0 }, It { static_cast<uint_t>(num_vertices(g)) });
}

template<typename Task>
auto num_vertices(const BackwardExplicitProjection<Task>& g)
{
    return g.num_vertices();
}

template<typename Task>
auto vertices(const BackwardExplicitProjection<Task>& g)
{
    return vertices(g.g());
}

/**
 * IncidenceGraph
 */

template<typename Task>
auto out_edges(uint_t v, const ExplicitProjection<Task>& g)
{
    const auto& r = g.adj_lists().at(v);
    return std::make_pair(r.begin(), r.end());
}

template<typename Task>
auto source(uint_t e, const ExplicitProjection<Task>& g)
{
    return g.transitions().at(e).src;
}

template<typename Task>
auto target(uint_t e, const ExplicitProjection<Task>& g)
{
    return g.transitions().at(e).dst;
}

template<typename Task>
auto out_degree(uint_t v, const ExplicitProjection<Task>& g)
{
    return g.adj_lists().at(v).size();
}

template<typename Task>
auto out_edges(uint_t v, const BackwardExplicitProjection<Task>& g)
{
    const auto& r = g.adj_lists().at(v);
    return std::make_pair(r.begin(), r.end());
}

template<typename Task>
auto source(uint_t e, const BackwardExplicitProjection<Task>& g)
{
    return g.transitions().at(e).dst;
}

template<typename Task>
auto target(uint_t e, const BackwardExplicitProjection<Task>& g)
{
    return g.transitions().at(e).src;
}

template<typename Task>
auto out_degree(uint_t v, const BackwardExplicitProjection<Task>& g)
{
    return g.adj_lists().at(v).size();
}

}

namespace boost
{

/// @private
/// Traits for a graph that are needed for the boost graph library.
template<typename Task>
struct graph_traits<::tyr::planning::ExplicitProjection<Task>>
{
    struct vertex_list_and_incidence_graph_tag : public vertex_list_graph_tag, public incidence_graph_tag
    {
    };

    // boost::GraphConcept
    using vertex_descriptor = ::tyr::uint_t;
    using edge_descriptor = ::tyr::uint_t;
    using directed_category = directed_tag;
    using edge_parallel_category = allow_parallel_edge_tag;
    using traversal_category = vertex_list_and_incidence_graph_tag;
    // boost::VertexListGraph
    using vertex_iterator = boost::counting_iterator<::tyr::uint_t>;
    using vertices_size_type = size_t;
    // boost::IncidenceGraph
    using out_edge_iterator = typename std::vector<::tyr::uint_t>::const_iterator;
    using degree_size_type = size_t;
    // boost::strong_components
    constexpr static vertex_descriptor null_vertex() { return std::numeric_limits<vertex_descriptor>::max(); }
};

/// @private
/// Traits for a graph that are needed for the boost graph library.
template<typename Task>
struct graph_traits<::tyr::planning::BackwardExplicitProjection<Task>>
{
    struct vertex_list_and_incidence_graph_tag : public vertex_list_graph_tag, public incidence_graph_tag
    {
    };

    // boost::GraphConcept
    using vertex_descriptor = ::tyr::uint_t;
    using edge_descriptor = ::tyr::uint_t;
    using directed_category = directed_tag;
    using edge_parallel_category = allow_parallel_edge_tag;
    using traversal_category = vertex_list_and_incidence_graph_tag;
    // boost::VertexListGraph
    using vertex_iterator = boost::counting_iterator<::tyr::uint_t>;
    using vertices_size_type = size_t;
    // boost::IncidenceGraph
    using out_edge_iterator = typename std::vector<::tyr::uint_t>::const_iterator;
    using degree_size_type = size_t;
    // boost::strong_components
    constexpr static vertex_descriptor null_vertex() { return std::numeric_limits<vertex_descriptor>::max(); }
};

}

namespace tyr::planning
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
