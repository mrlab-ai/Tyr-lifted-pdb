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

#include "tyr/planning/lifted_task/abstractions/projection_generator.hpp"

#include "tyr/common/declarations.hpp"
#include "tyr/formalism/overlay_repository.hpp"
#include "tyr/formalism/planning/builder.hpp"
#include "tyr/formalism/planning/declarations.hpp"
#include "tyr/formalism/planning/merge.hpp"
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/formalism/planning/views.hpp"
#include "tyr/planning/abstractions/pattern_generator.hpp"
#include "tyr/planning/abstractions/projection_generator.hpp"
#include "tyr/planning/declarations.hpp"
#include "tyr/planning/lifted_task.hpp"

namespace f = tyr::formalism;
namespace fp = tyr::formalism::planning;

namespace tyr::planning
{

static Index<fp::Task> create_projected_task(View<Index<fp::Task>, f::OverlayRepository<fp::Repository>> task,
                                             f::OverlayRepository<f::OverlayRepository<fp::Repository>, fp::Repository>& destination,
                                             const Pattern& pattern)
{
    auto builder = fp::Builder();
    auto context = fp::MergeContext<f::OverlayRepository<f::OverlayRepository<fp::Repository>, fp::Repository>>(builder, destination);
    auto projected_task_ptr = builder.get_builder<fp::Task>();
    auto& projected_task = *projected_task_ptr;
    projected_task.clear();

    for (const auto predicate : task.get_domain().get_predicates<f::StaticTag>()) {}
    // program.static_predicates.push_back(fp::merge_p2d(predicate, context).first);

    for (const auto predicate : task.get_domain().get_predicates<f::FluentTag>()) {}
    // program.fluent_predicates.push_back(fp::merge_p2d(predicate, context).first);

    for (const auto predicate : task.get_domain().get_predicates<f::DerivedTag>()) {}
    // program.fluent_predicates.push_back(
    //     fp::merge_p2d<f::DerivedTag, f::OverlayRepository<fp::Repository>, fd::Repository, f::FluentTag>(predicate, context).first);

    for (const auto predicate : task.get_derived_predicates()) {}
    // program.fluent_predicates.push_back(
    //     fp::merge_p2d<f::DerivedTag, f::OverlayRepository<fp::Repository>, fd::Repository, f::FluentTag>(predicate, context).first);

    for (const auto function : task.get_domain().get_functions<f::StaticTag>()) {}
    // program.static_functions.push_back(fp::merge_p2d(function, context).first);

    for (const auto function : task.get_domain().get_functions<f::FluentTag>()) {}
    // program.fluent_functions.push_back(fp::merge_p2d(function, context).first);

    // We can ignore auxiliary function total-cost because it never occurs in a condition

    for (const auto object : task.get_domain().get_constants()) {}
    // program.objects.push_back(fp::merge_p2d(object, context).first);
    for (const auto object : task.get_objects()) {}
    // program.objects.push_back(fp::merge_p2d(object, context).first);

    for (const auto atom : task.get_atoms<f::StaticTag>()) {}
    // program.static_atoms.push_back(fp::merge_p2d(atom, context).first);

    for (const auto atom : task.get_atoms<f::FluentTag>()) {}
    // program.fluent_atoms.push_back(fp::merge_p2d(atom, context).first);

    for (const auto fterm_value : task.get_fterm_values<f::StaticTag>()) {}
    // program.static_fterm_values.push_back(fp::merge_p2d(fterm_value, context).first);

    for (const auto fterm_value : task.get_fterm_values<f::FluentTag>()) {}
    //.fluent_fterm_values.push_back(fp::merge_p2d(fterm_value, context).first);

    for (const auto action : task.get_domain().get_actions()) {}
    // translate_action_to_delete_free_rules(action, program, context, predicate_to_actions);

    for (const auto axiom : task.get_domain().get_axioms()) {}
    // translate_axiom_to_delete_free_axiom_rules(axiom, program, context, predicate_to_axioms);

    for (const auto axiom : task.get_axioms()) {}
    // translate_axiom_to_delete_free_axiom_rules(axiom, program, context, predicate_to_axioms);

    canonicalize(projected_task);
    return destination.get_or_create(projected_task, builder.get_buffer()).first;
}

ProjectionGenerator<LiftedTask>::ProjectionGenerator(LiftedTask& task, const PatternCollection& patterns) : m_task(task), m_patterns(patterns) {}

void ProjectionGenerator<LiftedTask>::generate()
{
    for (const auto& pattern : m_patterns)
    {
        // TODO: create projected task
        auto repository = fp::Repository();
        auto overlay_repository = f::OverlayRepository<f::OverlayRepository<fp::Repository>, fp::Repository>(*m_task.get_repository(), repository);
        auto projected_task = create_projected_task(m_task.get_task(), overlay_repository, pattern);
        // TODO: create lifted projected task
        // TODO: ground lifted projected task
        // TODO: expand state space
    }
}

}
