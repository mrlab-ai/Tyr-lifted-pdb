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
#include "tyr/planning/abstractions/pattern_generator.hpp"
#include "tyr/planning/abstractions/projection_generator.hpp"
#include "tyr/planning/declarations.hpp"
#include "tyr/planning/lifted_task.hpp"

namespace tyr::planning
{

template<>
class ProjectionGenerator<LiftedTask>
{
public:
    ProjectionGenerator(LiftedTask& task, const PatternCollection& patterns);

    void generate()
    {
        for (const auto& pattern : m_patterns)
        {
            // TODO: create projected task
            for (const auto& fact : pattern) {}
            // TODO: create lifted projected task
            // TODO: ground lifted projected task
            // TODO: expand state space
        }
    }

private:
    LiftedTask& m_task;
    const PatternCollection& m_patterns;
};

}

#endif
