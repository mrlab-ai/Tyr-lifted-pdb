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

struct ProjectionGeneratorInstrumentation
{
    size_t condition_join_calls = 0;
    size_t selected_static_literals = 0;
    size_t selected_fluent_literals = 0;
    size_t selected_zero_candidate_literals = 0;
    size_t static_candidate_atoms_tried = 0;
    size_t fluent_candidate_atoms_tried = 0;
    size_t fluent_visible_branches = 0;
    size_t fluent_hidden_branches = 0;
    size_t condition_binding_callbacks = 0;
    size_t unifier_queries = 0;
    size_t change_unification_calls = 0;
    size_t positive_effect_candidates_tried = 0;
    size_t negative_effect_candidates_tried = 0;
    size_t effect_condition_fire_attempts = 0;
    size_t verified_binding_callbacks = 0;
    size_t patterns_processed = 0;
    size_t abstract_states_created = 0;
    size_t action_contexts_created = 0;
    size_t state_pair_checks = 0;
    size_t self_state_pairs_skipped = 0;
    size_t action_pair_checks = 0;
    size_t project_task_ns = 0;
    size_t project_fdr_context_ns = 0;
    size_t project_formalism_task_ns = 0;
    size_t project_lifted_task_create_ns = 0;
    size_t abstract_states_ns = 0;
    size_t projection_context_ns = 0;
    size_t abstract_state_infos_ns = 0;
    size_t action_contexts_ns = 0;
    size_t transition_loop_ns = 0;
    size_t transition_generation_ns = 0;
    size_t projection_assembly_ns = 0;
    size_t total_projection_ns = 0;
};

void reset_projection_generator_instrumentation();
ProjectionGeneratorInstrumentation get_projection_generator_instrumentation();
void add_projection_generator_project_task_breakdown(size_t fdr_context_ns, size_t formalism_task_ns, size_t lifted_task_create_ns);

template<>
class ProjectionGenerator<LiftedTag>
{
public:
    ProjectionGenerator(std::shared_ptr<const Task<LiftedTag>> task, PatternCollection patterns);

    ProjectionAbstractionList<LiftedTag> generate();

private:
    std::shared_ptr<const Task<LiftedTag>> m_task;
    PatternCollection m_patterns;
};

}

#endif
