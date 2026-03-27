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

#include "tyr/planning/ground_task/state_storage/fact_tree_compression.hpp"

#include "tyr/planning/ground_task.hpp"
#include "tyr/planning/ground_task/state_storage/context.hpp"

namespace tyr::planning
{

FactStorageBackend<GroundTask, TreeCompression>::FactStorageBackend(StateStorageContext<GroundTask, TreeCompression>& ctx) :
    m_array_set(ctx.fluent_array_set),
    m_infos(ctx.fluent_infos)
{
}

typename FactStorageBackend<GroundTask, TreeCompression>::Packed
FactStorageBackend<GroundTask, TreeCompression>::insert(const typename FactStorageBackend<GroundTask, TreeCompression>::Unpacked& unpacked)
{
}

void FactStorageBackend<GroundTask, TreeCompression>::unpack(const typename FactStorageBackend<GroundTask, TreeCompression>::Packed& packed,
                                                             typename FactStorageBackend<GroundTask, TreeCompression>::Unpacked& unpacked)
{
}

}
