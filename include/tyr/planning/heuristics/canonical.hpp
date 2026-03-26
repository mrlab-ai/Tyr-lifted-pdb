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

#ifndef TYR_PLANNING_HEURISTICS_CANONICAL_HPP_
#define TYR_PLANNING_HEURISTICS_CANONICAL_HPP_

#include "tyr/planning/abstractions/explicit_projection.hpp"
#include "tyr/planning/heuristic.hpp"

#include <memory>
#include <vector>

namespace tyr::planning
{

template<typename Task>
class CanonicalHeuristic : public Heuristic<Task>
{
private:
    std::vector<std::vector<std::shared_ptr<Heuristic<Task>>>> m_additive_components;

public:
    explicit CanonicalHeuristic(const std::vector<ProjectionAbstractionList<Task>>& projections);

    static std::shared_ptr<CanonicalHeuristic<Task>> create(const std::vector<ProjectionAbstractionList<Task>>& projections);

    float_t evaluate(const StateView<Task>& state) override;
};

}

#endif
