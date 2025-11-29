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

#ifndef TYR_PLANNING_TASK_HPP_
#define TYR_PLANNING_TASK_HPP_

#include "tyr/common/indexed_hash_set.hpp"
#include "tyr/common/shared_object_pool.hpp"
#include "tyr/formalism/formalism.hpp"
#include "tyr/planning/packed_state.hpp"
#include "tyr/planning/state.hpp"
#include "tyr/planning/state_index.hpp"
#include "tyr/planning/unpacked_state.hpp"

#include <gtl/phmap.hpp>
#include <valla/valla.hpp>

namespace tyr::planning
{

class Task
{
public:
    Task(std::shared_ptr<formalism::Repository> repository, Index<formalism::planning::Task> task_index);

    State get_state(StateIndex state_index);

    StateIndex register_state(const UnpackedState& state);

    View<Index<formalism::planning::Task>, formalism::Repository> get_task() const;

    // TODO: get_initial_node()

private:
    std::shared_ptr<formalism::Repository> m_repository;
    Index<formalism::planning::Task> m_task_index;

    // States
    valla::IndexedHashSet<valla::Slot<uint_t>, uint_t> m_uint_nodes;
    valla::IndexedHashSet<float_t, uint_t> m_float_nodes;
    IndexedHashSet<PackedState, StateIndex> m_packed_states;
    SharedObjectPool<UnpackedState> m_unpacked_state_pool;

    // TODO: initial node
};

}

#endif
