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

#include "tyr/planning/heuristics/canonical.hpp"

#include "tyr/planning/ground_task.hpp"
#include "tyr/planning/lifted_task.hpp"

namespace tyr::planning
{
template<typename Task>
CanonicalHeuristic<Task>::CanonicalHeuristic(const std::vector<ProjectionAbstractionList<Task>>& projections) : m_additive_components()
{
}

template<typename Task>
std::shared_ptr<CanonicalHeuristic<Task>> CanonicalHeuristic<Task>::create(const std::vector<ProjectionAbstractionList<Task>>& projections)
{
    return std::make_shared<CanonicalHeuristic<Task>>(std::move(projections));
}

template<typename Task>
float_t CanonicalHeuristic<Task>::evaluate(const StateView<Task>& state)
{
    float_t h = 0;
    for (const auto& additive_component : m_additive_components)
    {
        float_t h_sum = 0;
        for (const auto& h_i : additive_component)
            h_sum += h_i->evaluate(state);

        h = std::max(h, h_sum);
    }
    return h;
}

template class CanonicalHeuristic<LiftedTask>;
template class CanonicalHeuristic<GroundTask>;

}
