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

#ifndef TYR_PLANNING_LIFTED_TASK_ABSTRACTIONS_PROJECTION_GENERATOR_HPP_
#define TYR_PLANNING_LIFTED_TASK_ABSTRACTIONS_PROJECTION_GENERATOR_HPP_

#include "tyr/common/declarations.hpp"
#include "tyr/formalism/planning/declarations.hpp"
#include "tyr/formalism/planning/fdr_fact_view.hpp"
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/planning/abstractions/explicit_projection.hpp"
#include "tyr/planning/abstractions/pattern_generator.hpp"
#include "tyr/planning/abstractions/projection_generator.hpp"
#include "tyr/planning/declarations.hpp"
#include "tyr/planning/lifted_task.hpp"

namespace f = tyr::formalism;
namespace fp = tyr::formalism::planning;

namespace tyr::planning
{

// Phase 4a: ordering strategy for positive fluent precondition literals during
// projection-transition enumeration. Selectivity is the optimized default.
enum class FluentLiteralOrder
{
    Declaration,
    Selectivity,
};

// Phase 4b: whether to build a per-src-state predicate-keyed index over visible
// fluent atoms (mirrors StaticAtomIndex on the fluent side). On is the optimized
// default.
enum class SrcAtomsIndex
{
    Off,
    On,
};

struct ProjectionOptions
{
    FluentLiteralOrder fluent_literal_order = FluentLiteralOrder::Selectivity;
    SrcAtomsIndex src_atoms_index = SrcAtomsIndex::On;
};

template<>
class ProjectionGenerator<LiftedTag>
{
public:
    ProjectionGenerator(std::shared_ptr<const Task<LiftedTag>> task, PatternCollection patterns, ProjectionOptions options = {});

    ProjectionAbstractionList<LiftedTag> generate();

private:
    std::shared_ptr<const Task<LiftedTag>> m_task;
    PatternCollection m_patterns;
    ProjectionOptions m_options;
};

}

#endif
