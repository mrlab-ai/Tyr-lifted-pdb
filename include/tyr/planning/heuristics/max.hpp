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

#ifndef TYR_PLANNING_HEURISTICS_MAX_HPP_
#define TYR_PLANNING_HEURISTICS_MAX_HPP_

#include "tyr/planning/heuristic.hpp"

#include <memory>
#include <vector>

namespace tyr::planning
{

template<typename Task>
class MaxHeuristic : public Heuristic<Task>
{
private:
    std::vector<std::shared_ptr<Heuristic<Task>>> m_components;

public:
    explicit MaxHeuristic(std::vector<std::shared_ptr<Heuristic<Task>>> components);

    static std::shared_ptr<MaxHeuristic<Task>> create(std::vector<std::shared_ptr<Heuristic<Task>>> components);

    float_t evaluate(const StateView<Task>& state) override;
};

}

#endif
