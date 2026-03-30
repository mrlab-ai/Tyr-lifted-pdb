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

#include "tyr/planning/heuristics/projection_abstraction.hpp"

#include "tyr/graphs/bgl_algorithms.hpp"
#include "tyr/planning/ground_task.hpp"
#include "tyr/planning/lifted_task.hpp"

namespace tyr::planning
{

template<TaskKind Kind>
ProjectionAbstractionHeuristic<Kind>::ProjectionAbstractionHeuristic(const ProjectionAbstraction<Kind>& projection) :
    m_distances(),
    m_mapping(projection.get_mapping())
{
    auto weights = std::vector<float_t>(projection.get_forward().num_edges(), float_t { 1 });

    const auto [predecessors, distances] = graphs::dijkstra_shortest_paths(projection.get_backward(),
                                                                           weights,
                                                                           projection.get_backward().goal_vertices().begin(),
                                                                           projection.get_backward().goal_vertices().end());

    m_distances = std::move(distances);
}

template<TaskKind Kind>
std::shared_ptr<ProjectionAbstractionHeuristic<Kind>> ProjectionAbstractionHeuristic<Kind>::create(const ProjectionAbstraction<Kind>& projection)
{
    return std::make_shared<ProjectionAbstractionHeuristic<Kind>>(projection);
}

template<TaskKind Kind>
float_t ProjectionAbstractionHeuristic<Kind>::evaluate(const StateView<Kind>& state)
{
    return m_distances.at(m_mapping.map_state(state));
}

template class ProjectionAbstractionHeuristic<LiftedTag>;
template class ProjectionAbstractionHeuristic<GroundTag>;

}
