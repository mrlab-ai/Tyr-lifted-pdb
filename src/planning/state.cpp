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

#include "tyr/planning/state.hpp"

namespace tyr::planning
{

State::State(Task& task, SharedObjectPoolPtr<UnpackedState> unpacked) noexcept : m_unpacked(unpacked), m_task(&task) {}

StateIndex State::get_index() const noexcept { return m_unpacked->get_index(); }

template<formalism::IsFactTag T>
const boost::dynamic_bitset<>& State::get_atoms() const noexcept
{
    return m_unpacked->get_atoms<T>();
}

const std::vector<float_t>& State::get_numeric_variables() const noexcept { return m_unpacked->get_numeric_variables(); }

Task& State::get_task() noexcept { return *m_task; }

}
