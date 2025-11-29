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

#ifndef TYR_PLANNING_STATE_HPP_
#define TYR_PLANNING_STATE_HPP_

#include "tyr/common/shared_object_pool.hpp"
#include "tyr/planning/declarations.hpp"
#include "tyr/planning/unpacked_state.hpp"

namespace tyr::planning
{
class State
{
public:
    State(Task& task, SharedObjectPoolPtr<UnpackedState> unpacked) noexcept;

    StateIndex get_index() const noexcept;

    template<formalism::IsFactTag T>
    const boost::dynamic_bitset<>& get_atoms() const noexcept;

    const std::vector<float_t>& get_numeric_variables() const noexcept;

    Task& get_task() noexcept;

private:
    SharedObjectPoolPtr<UnpackedState> m_unpacked;
    Task* m_task;
};
}

#endif
