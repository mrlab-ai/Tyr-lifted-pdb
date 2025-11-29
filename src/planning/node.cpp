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

#include "tyr/planning/node.hpp"

#include "tyr/common/config.hpp"
#include "tyr/planning/task.hpp"

namespace tyr::planning
{

Node::Node(StateIndex state_index, float_t state_metric, planning::Task& task) noexcept :
    m_task(&task),
    m_state_metric(state_metric),
    m_state_index(state_index)
{
}

// Get the unpacked state
State Node::get_state() const { return m_task->get_state(m_state_index); }

Task& Node::get_task() noexcept { return *m_task; }

float_t Node::get_state_metric() const noexcept { return m_state_metric; }

StateIndex Node::get_state_index() const noexcept { return m_state_index; }

}
