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

#include "tyr/planning/heuristics/max.hpp"

#include "tyr/planning/ground_task.hpp"
#include "tyr/planning/lifted_task.hpp"

namespace tyr::planning
{
template<TaskKind Kind>
MaxHeuristic<Kind>::MaxHeuristic(std::vector<std::shared_ptr<Heuristic<Kind>>> components) : m_components(std::move(components))
{
}

template<TaskKind Kind>
std::shared_ptr<MaxHeuristic<Kind>> MaxHeuristic<Kind>::create(std::vector<std::shared_ptr<Heuristic<Kind>>> components)
{
    return std::make_shared<MaxHeuristic<Kind>>(std::move(components));
}

template<TaskKind Kind>
float_t MaxHeuristic<Kind>::evaluate(const StateView<Kind>& state)
{
    float_t h = 0;
    for (const auto& h_i : m_components)
        h = std::max(h, h_i->evaluate(state));
    return h;
}

template class MaxHeuristic<LiftedTag>;
template class MaxHeuristic<GroundTag>;

}
