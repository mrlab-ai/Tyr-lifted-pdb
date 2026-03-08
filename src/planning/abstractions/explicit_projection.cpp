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

#include "tyr/planning/abstractions/explicit_projection.hpp"

#include "tyr/formalism/planning/repository.hpp"
#include "tyr/formalism/planning/views.hpp"
#include "tyr/planning/ground_task/state.hpp"
#include "tyr/planning/lifted_task/state.hpp"

namespace tyr::planning
{
static_assert(graphs::VertexListGraph<ProjectionAbstraction<LiftedTask>>);
static_assert(graphs::VertexListGraph<ProjectionAbstraction<GroundTask>>);

static_assert(graphs::IncidenceGraph<ProjectionAbstraction<LiftedTask>>);
static_assert(graphs::IncidenceGraph<ProjectionAbstraction<GroundTask>>);

static_assert(graphs::ContiguousIndexingMode<ProjectionAbstraction<LiftedTask>>);
static_assert(graphs::ContiguousIndexingMode<ProjectionAbstraction<GroundTask>>);
}