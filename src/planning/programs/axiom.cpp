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
#include "tyr/formalism/datalog/datas.hpp"
#include "tyr/formalism/datalog/formatter.hpp"
#include "tyr/formalism/datalog/repository.hpp"
#include "tyr/formalism/datalog/views.hpp"
#include "tyr/formalism/overlay_repository.hpp"
#include "tyr/formalism/planning/formatter.hpp"
#include "tyr/formalism/planning/merge_datalog.hpp"
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/formalism/planning/views.hpp"

namespace f = tyr::formalism;
namespace fp = tyr::formalism::planning;
namespace fd = tyr::formalism::datalog;

namespace tyr::planning
{
static void process_axiom_body(View<Index<fp::ConjunctiveCondition>, f::OverlayRepository<fp::Repository>> axiom_body,
                               fp::MergeDatalogContext<fd::Repository>& context,
                               Data<fd::ConjunctiveCondition>& conj_cond)
{
    for (const auto literal : axiom_body.get_literals<f::StaticTag>())
        conj_cond.static_literals.push_back(fp::merge_p2d(literal, context).first);

    for (const auto literal : axiom_body.get_literals<f::FluentTag>())
        conj_cond.fluent_literals.push_back(fp::merge_p2d(literal, context).first);

    for (const auto literal : axiom_body.get_literals<f::DerivedTag>())
        conj_cond.fluent_literals.push_back(
            fp::merge_p2d<f::DerivedTag, f::OverlayRepository<fp::Repository>, fd::Repository, f::FluentTag>(literal, context).first);

    for (const auto numeric_constraint : axiom_body.get_numeric_constraints())
        conj_cond.numeric_constraints.push_back(fp::merge_p2d(numeric_constraint, context));
}

static auto create_axiom_rule(View<Index<fp::Axiom>, f::OverlayRepository<fp::Repository>> axiom, fp::MergeDatalogContext<fd::Repository>& context)
{
    auto rule_ptr = context.builder.get_builder<fd::Rule>();
    auto& rule = *rule_ptr;
    rule.clear();

    auto conj_cond_ptr = context.builder.get_builder<fd::ConjunctiveCondition>();
    auto& conj_cond = *conj_cond_ptr;
    conj_cond.clear();

    for (const auto variable : axiom.get_variables())
        conj_cond.variables.push_back(fp::merge_p2d(variable, context).first);

    process_axiom_body(axiom.get_body(), context, conj_cond);

    canonicalize(conj_cond);
    const auto new_conj_cond = context.destination.get_or_create(conj_cond, context.builder.get_buffer()).first;

    rule.body = new_conj_cond;

    const auto new_head = fp::merge_p2d<f::DerivedTag, f::OverlayRepository<fp::Repository>, fd::Repository, f::FluentTag>(axiom.get_head(), context).first;

    rule.head = new_head;

    canonicalize(rule);
    return context.destination.get_or_create(rule, context.builder.get_buffer());
}

static View<Index<fd::Program>, fd::Repository> create(View<Index<fp::Task>, f::OverlayRepository<fp::Repository>> task,
                                                       AxiomEvaluatorProgram::PredicateToPredicateMapping& predicate_to_predicate_mapping,
                                                       fd::Repository& repository)
{
    auto merge_cache = fp::MergeDatalogCache();
    auto builder = fd::Builder();
    auto context = fp::MergeDatalogContext<fd::Repository>(builder, repository, merge_cache);
    auto program_ptr = builder.get_builder<fd::Program>();
    auto& program = *program_ptr;
    program.clear();

    for (const auto predicate : task.get_domain().get_predicates<f::StaticTag>())
        program.static_predicates.push_back(fp::merge_p2d(predicate, context).first);

    for (const auto predicate : task.get_domain().get_predicates<f::FluentTag>())
        program.fluent_predicates.push_back(fp::merge_p2d(predicate, context).first);

    for (const auto predicate : task.get_domain().get_predicates<f::DerivedTag>())
    {
        const auto new_predicate = fp::merge_p2d<f::DerivedTag, f::OverlayRepository<fp::Repository>, fd::Repository, f::FluentTag>(predicate, context).first;

        predicate_to_predicate_mapping.emplace(new_predicate, predicate.get_index());

        program.fluent_predicates.push_back(new_predicate);
    }

    for (const auto predicate : task.get_derived_predicates())
    {
        const auto new_predicate = fp::merge_p2d<f::DerivedTag, f::OverlayRepository<fp::Repository>, fd::Repository, f::FluentTag>(predicate, context).first;

        predicate_to_predicate_mapping.emplace(new_predicate, predicate.get_index());

        program.fluent_predicates.push_back(new_predicate);
    }

    for (const auto function : task.get_domain().get_functions<f::StaticTag>())
        program.static_functions.push_back(fp::merge_p2d(function, context).first);

    for (const auto function : task.get_domain().get_functions<f::FluentTag>())
        program.fluent_functions.push_back(fp::merge_p2d(function, context).first);

    // We can ignore auxiliary function total-cost because it never occurs in a condition

    for (const auto object : task.get_domain().get_constants())
        program.objects.push_back(fp::merge_p2d(object, context).first);
    for (const auto object : task.get_objects())
        program.objects.push_back(fp::merge_p2d(object, context).first);

    for (const auto atom : task.get_atoms<f::StaticTag>())
        program.static_atoms.push_back(fp::merge_p2d(atom, context).first);

    for (const auto atom : task.get_atoms<f::FluentTag>())
        program.fluent_atoms.push_back(fp::merge_p2d(atom, context).first);

    for (const auto fterm_value : task.get_fterm_values<f::StaticTag>())
        program.static_fterm_values.push_back(fp::merge_p2d(fterm_value, context).first);

    for (const auto axiom : task.get_domain().get_axioms())
        program.rules.push_back(create_axiom_rule(axiom, context).first);

    for (const auto axiom : task.get_axioms())
        program.rules.push_back(create_axiom_rule(axiom, context).first);

    canonicalize(program);
    return make_view(repository.get_or_create(program, builder.get_buffer()).first, context.destination);
}

AxiomEvaluatorProgram::AxiomEvaluatorProgram(View<Index<fp::Task>, f::OverlayRepository<fp::Repository>> task) :
    m_predicate_to_predicate(),
    m_repository(std::make_shared<fd::Repository>()),
    m_program(create(task, m_predicate_to_predicate, *m_repository)),
    m_domains(analysis::compute_variable_domains(m_program)),
    m_strata(analysis::compute_rule_stratification(m_program)),
    m_listeners(analysis::compute_listeners(m_strata, m_program.get_context()))
{
    // std::cout << m_program << std::endl;
    m_repository->notify_num_predicates<f::FluentTag>(m_program.get_predicates<f::FluentTag>().size());
    m_repository->notify_num_functions<f::FluentTag>(m_program.get_functions<f::FluentTag>().size());
}

const AxiomEvaluatorProgram::PredicateToPredicateMapping& AxiomEvaluatorProgram::get_predicate_to_predicate_mapping() const noexcept
{
    return m_predicate_to_predicate;
}

View<Index<fd::Program>, fd::Repository> AxiomEvaluatorProgram::get_program() const noexcept { return m_program; }

const fd::RepositoryPtr& AxiomEvaluatorProgram::get_repository() const noexcept { return m_repository; }

const analysis::ProgramVariableDomains& AxiomEvaluatorProgram::get_domains() const noexcept { return m_domains; }

const analysis::RuleStrata& AxiomEvaluatorProgram::get_strata() const noexcept { return m_strata; }

const analysis::ListenerStrata& AxiomEvaluatorProgram::get_listeners() const noexcept { return m_listeners; }
}
