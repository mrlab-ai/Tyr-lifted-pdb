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
#include "tyr/planning/abstractions/pattern_generator.hpp"
#include "tyr/planning/state_view.hpp"

#include <boost/graph/graph_concepts.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <ranges>

namespace tyr::planning
{

struct Transition
{
    // TODO: get rid of actions.
    formalism::planning::GroundActionView label;
    uint_t src;
    uint_t dst;
};

using TransitionList = std::vector<Transition>;

template<typename Task>
class ProjectionMapping
{
public:
    using ActionMapping = UnorderedMap<formalism::planning::ActionView, formalism::planning::ActionView>;

    explicit ProjectionMapping(Pattern pattern, ActionMapping projected_to_original_action) :
        m_pattern(std::move(pattern)),
        m_projected_to_original_action(std::move(projected_to_original_action))
    {
    }

    uint_t map_state(const StateView<Task>& state) const noexcept
    {
        // Mimics the indexing obtained via itertools::for_each_boolean_vector used during construction of the ForwardProjectionAbstraction.
        const auto k = m_pattern.size();
        auto r = uint_t(0);
        for (uint_t i = 0; i < k; ++i)
        {
            const auto fact = m_pattern.facts[i];
            const auto variable = fact.get_variable();
            const auto value = fact.get_value();
            if (state.get(variable) == value)
                r |= (uint_t(1) << i);
        }
        return r;
    }

    formalism::planning::ActionView get_original_action(formalism::planning::ActionView projected_action) const noexcept
    {
        return m_projected_to_original_action.at(projected_action);
    }

private:
    Pattern m_pattern;
    ActionMapping m_projected_to_original_action;
};

template<typename Task>
class ForwardProjectionAbstraction
{
public:
    using IndexingMode = graphs::ContiguousIndexingTag;

    static boost::dynamic_bitset<> compute_state_changing_transitions(const TransitionList& transitions)
    {
        auto result = boost::dynamic_bitset<>(transitions.size());
        for (uint_t t = 0; t < transitions.size(); ++t)
        {
            const auto& transition = transitions[t];
            if (transition.src != transition.dst)
                result.set(t);
        }
        return result;
    }

    ForwardProjectionAbstraction(ProjectionMapping<Task> mapping,
                                 std::vector<StateView<Task>> vertices,
                                 TransitionList transitions,
                                 std::vector<std::vector<uint_t>> adj_lists,
                                 std::vector<uint_t> goal_vertices) :
        m_mapping(std::move(mapping)),
        m_vertices(std::move(vertices)),
        m_transitions(std::move(transitions)),
        m_adj_lists(std::move(adj_lists)),
        m_goal_vertices(std::move(goal_vertices)),
        m_state_changing_transitions(compute_state_changing_transitions(m_transitions))
    {
    }

    const auto& get_mapping() const noexcept { return m_mapping; }
    auto num_vertices() const noexcept { return m_vertices.size(); }
    auto num_edges() const noexcept { return m_transitions.size(); }
    const auto& vertices() const noexcept { return m_vertices; }
    const auto& transitions() const noexcept { return m_transitions; }
    const auto& adj_lists() const noexcept { return m_adj_lists; }
    const auto& goal_vertices() const noexcept { return m_goal_vertices; }
    const auto& state_changing_transitions() const noexcept { return m_state_changing_transitions; }

private:
    ProjectionMapping<Task> m_mapping;
    // TODO: get rid of states.
    std::vector<StateView<Task>> m_vertices;
    TransitionList m_transitions;
    std::vector<std::vector<uint_t>> m_adj_lists;
    std::vector<uint_t> m_goal_vertices;
    boost::dynamic_bitset<> m_state_changing_transitions;
};

template<typename Task>
class BackwardProjectionAbstraction
{
public:
    using IndexingMode = graphs::ContiguousIndexingTag;

    explicit BackwardProjectionAbstraction(std::shared_ptr<const ForwardProjectionAbstraction<Task>> g) : m_g(g), m_adj_lists(g->num_vertices())
    {
        for (uint_t t = 0; t < g->num_edges(); ++t)
            m_adj_lists[g->transitions()[t].dst].push_back(t);
    }

    auto num_vertices() const noexcept { return m_g->num_vertices(); }
    auto num_edges() const noexcept { return m_g->num_edges(); }
    const auto& vertices() const noexcept { return m_g->vertices(); }
    const auto& transitions() const noexcept { return m_g->transitions(); }
    const auto& goal_vertices() const noexcept { return m_g->goal_vertices(); }
    const auto& adj_lists() const noexcept { return m_adj_lists; }
    const auto& g() const noexcept { return *m_g; }

private:
    std::shared_ptr<const ForwardProjectionAbstraction<Task>> m_g;

    std::vector<std::vector<uint_t>> m_adj_lists;
};

template<typename Task>
class ProjectionAbstraction
{
public:
    using IndexingMode = graphs::ContiguousIndexingTag;

    explicit ProjectionAbstraction(std::shared_ptr<const ForwardProjectionAbstraction<Task>> forward) : m_forward(std::move(forward)), m_backward(m_forward) {}

    const auto& get_mapping() const noexcept { return m_forward->get_mapping(); }
    const auto& state_changing_transitions() const noexcept { return m_forward->state_changing_transitions(); }
    const auto& transitions() const noexcept { return m_forward->transitions(); }
    const auto& get_forward() const noexcept { return *m_forward; }
    const auto& get_backward() const noexcept { return m_backward; }

private:
    std::shared_ptr<const ForwardProjectionAbstraction<Task>> m_forward;
    BackwardProjectionAbstraction<Task> m_backward;
};

template<typename Task>
using ProjectionAbstractionList = std::vector<ProjectionAbstraction<Task>>;

/**
 * VertexListGraph
 */

template<typename Task>
auto num_vertices(const ForwardProjectionAbstraction<Task>& g)
{
    return g.num_vertices();
}

template<typename Task>
auto vertices(const ForwardProjectionAbstraction<Task>& g)
{
    using It = boost::counting_iterator<uint_t>;
    return std::make_pair(It { 0 }, It { static_cast<uint_t>(num_vertices(g)) });
}

template<typename Task>
auto num_vertices(const BackwardProjectionAbstraction<Task>& g)
{
    return g.num_vertices();
}

template<typename Task>
auto vertices(const BackwardProjectionAbstraction<Task>& g)
{
    return vertices(g.g());
}

/**
 * IncidenceGraph
 */

template<typename Task>
auto out_edges(uint_t v, const ForwardProjectionAbstraction<Task>& g)
{
    const auto& r = g.adj_lists().at(v);
    return std::make_pair(r.begin(), r.end());
}

template<typename Task>
auto source(uint_t e, const ForwardProjectionAbstraction<Task>& g)
{
    return g.transitions().at(e).src;
}

template<typename Task>
auto target(uint_t e, const ForwardProjectionAbstraction<Task>& g)
{
    return g.transitions().at(e).dst;
}

template<typename Task>
auto out_degree(uint_t v, const ForwardProjectionAbstraction<Task>& g)
{
    return g.adj_lists().at(v).size();
}

template<typename Task>
auto out_edges(uint_t v, const BackwardProjectionAbstraction<Task>& g)
{
    const auto& r = g.adj_lists().at(v);
    return std::make_pair(r.begin(), r.end());
}

template<typename Task>
auto source(uint_t e, const BackwardProjectionAbstraction<Task>& g)
{
    return g.transitions().at(e).dst;
}

template<typename Task>
auto target(uint_t e, const BackwardProjectionAbstraction<Task>& g)
{
    return g.transitions().at(e).src;
}

template<typename Task>
auto out_degree(uint_t v, const BackwardProjectionAbstraction<Task>& g)
{
    return g.adj_lists().at(v).size();
}

}

namespace boost
{

/// @private
/// Traits for a graph that are needed for the boost graph library.
template<typename Task>
struct graph_traits<::tyr::planning::ForwardProjectionAbstraction<Task>>
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
struct graph_traits<::tyr::planning::BackwardProjectionAbstraction<Task>>
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

#endif
