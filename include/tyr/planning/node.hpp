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

#ifndef TYR_PLANNING_NODE_HPP_
#define TYR_PLANNING_NODE_HPP_

#include "tyr/common/config.hpp"
#include "tyr/planning/state.hpp"
#include "tyr/planning/state_index.hpp"

namespace tyr::planning
{
class Node
{
public:
    Node(StateIndex state_index, float_t state_metric, planning::Task& task) noexcept;

    State get_state() const;
    Task& get_task() noexcept;
    float_t get_state_metric() const noexcept;
    StateIndex get_state_index() const noexcept;

    // TODO: get_successor_states()
    // TODO: get_applicable_actions()

private:
    planning::Task* m_task;
    float_t m_state_metric;
    StateIndex m_state_index;
};
}

#endif
