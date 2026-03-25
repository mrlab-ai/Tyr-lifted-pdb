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
template<typename Task>
MaxHeuristic<Task>::MaxHeuristic(std::vector<std::shared_ptr<Heuristic<Task>>> components) : m_components(std::move(components))
{
}

template<typename Task>
std::shared_ptr<MaxHeuristic<Task>> MaxHeuristic<Task>::create(std::vector<std::shared_ptr<Heuristic<Task>>> components)
{
    return std::make_shared<MaxHeuristic<Task>>(std::move(components));
}

template<typename Task>
float_t MaxHeuristic<Task>::evaluate(const StateView<Task>& state)
{
    float_t h = 0;
    for (const auto& h_i : m_components)
        h = std::max(h, h_i->evaluate(state));
    return h;
}

template class MaxHeuristic<LiftedTask>;
template class MaxHeuristic<GroundTask>;

}
