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

#ifndef TYR_PLANNING_ABSTRACTIONS_PROJECTION_GENERATOR_HPP_
#define TYR_PLANNING_ABSTRACTIONS_PROJECTION_GENERATOR_HPP_

#include "tyr/common/declarations.hpp"
#include "tyr/planning/state.hpp"

namespace tyr::planning
{

template<typename Task>
class ProjectionGenerator
{
    static_assert(dependent_false<Task>::value, "PatternGenerator is not defined for type T.");
};

template<typename Task>
class ExplicitProjection
{
public:
    ExplicitProjection(std::vector<State<Task>> astates,
                       std::vector<std::vector<std::vector<View<Index<formalism::planning::GroundAction>, formalism::planning::Repository>>>> adj_matrix) :
        m_astates(std::move(astates)),
        m_adj_matrix(std::move(adj_matrix))
    {
    }

    const auto& astates() const noexcept { return m_astates; }
    const auto& adj_matrix() const noexcept { return m_adj_matrix; }

private:
    std::vector<State<Task>> m_astates;
    std::vector<std::vector<std::vector<View<Index<formalism::planning::GroundAction>, formalism::planning::Repository>>>> m_adj_matrix;
};

}

#endif
