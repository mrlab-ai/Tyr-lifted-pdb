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

#include "tyr/planning/programs/axiom.hpp"

#include "common.hpp"
#include "tyr/formalism/compile.hpp"
#include "tyr/formalism/merge.hpp"
#include "tyr/formalism/overlay_repository.hpp"
#include "tyr/formalism/repository.hpp"
#include "tyr/formalism/views.hpp"
#include "tyr/planning/lifted_task.hpp"

namespace tyr::planning
{

static View<Index<formalism::Program>, formalism::Repository>
create(const LiftedTask& task, AxiomEvaluatorProgram::PredicateToPredicateMapping& mapping, formalism::Repository& repository)
{
    auto merge_cache = formalism::MergeCache<formalism::OverlayRepository<formalism::Repository>, formalism::Repository>();
    auto compile_cache = formalism::CompileCache<formalism::OverlayRepository<formalism::Repository>, formalism::Repository>();
    auto builder = formalism::Builder();
    auto program_ptr = builder.get_builder<formalism::Program>();
    auto& program = *program_ptr;
    program.clear();

    for (const auto predicate : task.get_task().get_domain().get_predicates<formalism::StaticTag>())
    {
        program.static_predicates.push_back(formalism::merge(predicate, builder, repository, merge_cache).get_index());
    }

    for (const auto predicate : task.get_task().get_domain().get_predicates<formalism::FluentTag>())
    {
        program.fluent_predicates.push_back(formalism::merge(predicate, builder, repository, merge_cache).get_index());
    }

    for (const auto predicate : task.get_task().get_domain().get_predicates<formalism::DerivedTag>())
    {
        program.fluent_predicates.push_back(
            formalism::compile<formalism::DerivedTag, formalism::FluentTag>(predicate, builder, repository, compile_cache, merge_cache).get_index());
    }

    for (const auto predicate : task.get_task().get_derived_predicates())
    {
        program.fluent_predicates.push_back(
            formalism::compile<formalism::DerivedTag, formalism::FluentTag>(predicate, builder, repository, compile_cache, merge_cache).get_index());
    }

    for (const auto function : task.get_task().get_domain().get_functions<formalism::StaticTag>())
    {
        program.static_functions.push_back(formalism::merge(function, builder, repository, merge_cache).get_index());
    }

    for (const auto function : task.get_task().get_domain().get_functions<formalism::FluentTag>())
    {
        program.fluent_functions.push_back(formalism::merge(function, builder, repository, merge_cache).get_index());
    }

    // We can ignore auxiliary function total-cost because it never occurs in a condition

    for (const auto object : task.get_task().get_domain().get_constants())
    {
        program.objects.push_back(formalism::merge(object, builder, repository, merge_cache).get_index());
    }
    for (const auto object : task.get_task().get_objects())
    {
        program.objects.push_back(formalism::merge(object, builder, repository, merge_cache).get_index());
    }

    for (const auto atom : task.get_task().get_atoms<formalism::StaticTag>())
    {
        program.static_atoms.push_back(formalism::merge(atom, builder, repository, merge_cache).get_index());
    }

    for (const auto atom : task.get_task().get_atoms<formalism::FluentTag>())
    {
        program.fluent_atoms.push_back(formalism::merge(atom, builder, repository, merge_cache).get_index());
    }

    for (const auto fterm_value : task.get_task().get_fterm_values<formalism::StaticTag>())
    {
        program.static_fterm_values.push_back(formalism::merge(fterm_value, builder, repository, merge_cache).get_index());
    }

    for (const auto axiom : task.get_task().get_domain().get_axioms())
    {
        auto new_rule = create_axiom_rule(axiom, builder, repository, merge_cache, compile_cache);
        mapping.emplace(new_rule.get_head().get_predicate(), axiom.get_head().get_predicate());
        program.rules.push_back(new_rule.get_index());
    }

    for (const auto axiom : task.get_task().get_axioms())
    {
        auto new_rule = create_axiom_rule(axiom, builder, repository, merge_cache, compile_cache);
        mapping.emplace(new_rule.get_head().get_predicate(), axiom.get_head().get_predicate());
        program.rules.push_back(new_rule.get_index());
    }

    formalism::canonicalize(program);
    return repository.get_or_create(program, builder.get_buffer()).first;
}

AxiomEvaluatorProgram::AxiomEvaluatorProgram(const LiftedTask& task) :
    prediate_to_predicate(),
    m_repository(std::make_shared<formalism::Repository>()),
    m_program(create(task, prediate_to_predicate, *m_repository))
{
}

View<Index<formalism::Program>, formalism::Repository> AxiomEvaluatorProgram::get_program() const { return m_program; }

const formalism::RepositoryPtr& AxiomEvaluatorProgram::get_repository() const { return m_repository; }

}
