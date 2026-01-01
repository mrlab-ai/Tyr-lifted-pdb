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

#include "tyr/planning/programs/ground.hpp"

#include "common.hpp"
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

/**
 * Assume each pre and eff is a set of literals
 *
 * Action = [pre, [<c1_pre,c1_eff>,...,cn_pre,cn_eff>]]
 * App_pre :- pre
 * App_c1_pre :- App_pre and ci_pre    forall i=1,...,n
 * e :- App_ci_pre                     forall i=1,...,n forall e in ci_eff
 */

static void translate_action_to_delete_free_rules(View<Index<fp::Action>, f::OverlayRepository<fp::Repository>> action,
                                                  Data<fd::Program>& program,
                                                  fp::MergeDatalogContext<fd::Repository>& context,
                                                  GroundTaskProgram::AppPredicateToActionsMapping& predicate_to_actions_mapping)
{
    const auto applicability_predicate = create_applicability_predicate(action, context).first;

    predicate_to_actions_mapping[applicability_predicate].emplace_back(action.get_index());

    program.fluent_predicates.push_back(applicability_predicate);

    const auto applicability_rule = create_applicability_rule(action, context).first;

    program.rules.push_back(applicability_rule);

    for (const auto cond_eff : action.get_effects())
    {
        for (const auto literal : cond_eff.get_effect().get_literals())
        {
            if (!literal.get_polarity())
                continue;  /// ignore delete effects

            program.rules.push_back(
                create_cond_effect_rule(action, cond_eff, make_view(fp::merge_p2d(literal.get_atom(), context).first, context.destination), context).first);
        }
    }
}

static void translate_axiom_to_delete_free_axiom_rules(View<Index<fp::Axiom>, f::OverlayRepository<fp::Repository>> axiom,
                                                       Data<fd::Program>& program,
                                                       fp::MergeDatalogContext<fd::Repository>& context,
                                                       GroundTaskProgram::AppPredicateToAxiomsMapping& predicate_to_axioms_mapping)
{
    const auto applicability_predicate = create_applicability_predicate(axiom, context).first;

    program.fluent_predicates.push_back(applicability_predicate);

    predicate_to_axioms_mapping[applicability_predicate].emplace_back(axiom.get_index());

    const auto applicability_rule = create_applicability_rule(axiom, context).first;

    program.rules.push_back(applicability_rule);

    program.rules.push_back(
        create_effect_rule(
            axiom,
            make_view(fp::merge_p2d<f::DerivedTag, f::OverlayRepository<fp::Repository>, fd::Repository, f::FluentTag>(axiom.get_head(), context).first,
                      context.destination),
            context)
            .first);
}

static View<Index<fd::Program>, fd::Repository> create(View<Index<fp::Task>, f::OverlayRepository<fp::Repository>> task,
                                                       GroundTaskProgram::AppPredicateToActionsMapping& rule_to_actions_mapping,
                                                       GroundTaskProgram::AppPredicateToAxiomsMapping& predicate_to_axioms_mapping,
                                                       fd::Repository& destination)
{
    auto merge_cache = fp::MergeDatalogCache();
    auto builder = fd::Builder();
    auto context = fp::MergeDatalogContext<fd::Repository>(builder, destination, merge_cache);
    auto program_ptr = builder.get_builder<fd::Program>();
    auto& program = *program_ptr;
    program.clear();

    for (const auto predicate : task.get_domain().get_predicates<f::StaticTag>())
        program.static_predicates.push_back(fp::merge_p2d(predicate, context).first);

    for (const auto predicate : task.get_domain().get_predicates<f::FluentTag>())
        program.fluent_predicates.push_back(fp::merge_p2d(predicate, context).first);

    for (const auto predicate : task.get_domain().get_predicates<f::DerivedTag>())
        program.fluent_predicates.push_back(
            fp::merge_p2d<f::DerivedTag, f::OverlayRepository<fp::Repository>, fd::Repository, f::FluentTag>(predicate, context).first);

    for (const auto predicate : task.get_derived_predicates())
        program.fluent_predicates.push_back(
            fp::merge_p2d<f::DerivedTag, f::OverlayRepository<fp::Repository>, fd::Repository, f::FluentTag>(predicate, context).first);

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

    for (const auto fterm_value : task.get_fterm_values<f::FluentTag>())
        program.fluent_fterm_values.push_back(fp::merge_p2d(fterm_value, context).first);

    for (const auto action : task.get_domain().get_actions())
        translate_action_to_delete_free_rules(action, program, context, rule_to_actions_mapping);

    for (const auto axiom : task.get_domain().get_axioms())
        translate_axiom_to_delete_free_axiom_rules(axiom, program, context, predicate_to_axioms_mapping);

    for (const auto axiom : task.get_axioms())
        translate_axiom_to_delete_free_axiom_rules(axiom, program, context, predicate_to_axioms_mapping);

    canonicalize(program);
    return make_view(destination.get_or_create(program, builder.get_buffer()).first, destination);
}

GroundTaskProgram::GroundTaskProgram(View<Index<fp::Task>, f::OverlayRepository<fp::Repository>> task) :
    m_predicate_to_actions(),
    m_predicate_to_axioms(),
    m_repository(std::make_shared<fd::Repository>()),
    m_program(create(task, m_predicate_to_actions, m_predicate_to_axioms, *m_repository)),
    m_domains(analysis::compute_variable_domains(m_program)),
    m_strata(analysis::compute_rule_stratification(m_program)),
    m_listeners(analysis::compute_listeners(m_strata, m_program.get_context()))
{
    // std::cout << m_program << std::endl;
    m_repository->notify_num_predicates<f::FluentTag>(m_program.get_predicates<f::FluentTag>().size());
    m_repository->notify_num_functions<f::FluentTag>(m_program.get_functions<f::FluentTag>().size());
}

const GroundTaskProgram::AppPredicateToActionsMapping& GroundTaskProgram::get_predicate_to_actions_mapping() const noexcept { return m_predicate_to_actions; }

const GroundTaskProgram::AppPredicateToAxiomsMapping& GroundTaskProgram::get_predicate_to_axioms_mapping() const noexcept { return m_predicate_to_axioms; }

View<Index<fd::Program>, fd::Repository> GroundTaskProgram::get_program() const noexcept { return m_program; }

const fd::RepositoryPtr& GroundTaskProgram::get_repository() const noexcept { return m_repository; }

const analysis::ProgramVariableDomains& GroundTaskProgram::get_domains() const noexcept { return m_domains; }

const analysis::RuleStrata& GroundTaskProgram::get_strata() const noexcept { return m_strata; }

const analysis::ListenerStrata& GroundTaskProgram::get_listeners() const noexcept { return m_listeners; }

}
