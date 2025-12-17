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

#include "tyr/formalism/formatter.hpp"
#include "tyr/formalism/merge_datalog.hpp"
#include "tyr/formalism/overlay_repository.hpp"
#include "tyr/formalism/repository.hpp"
#include "tyr/formalism/views.hpp"
#include "tyr/planning/lifted_task.hpp"

using namespace tyr::formalism;

namespace tyr::planning
{

void append_from_condition(View<Index<FDRConjunctiveCondition>, OverlayRepository<Repository>> cond,
                           MergeContext<OverlayRepository<Repository>, Repository>& context,
                           Data<ConjunctiveCondition>& conj_cond)
{
    for (const auto literal : cond.template get_literals<StaticTag>())
        if (literal.get_polarity())
            conj_cond.static_literals.push_back(merge(literal, context).get_index());

    for (const auto literal : cond.template get_literals<FluentTag>())
        if (literal.get_polarity())
            conj_cond.fluent_literals.push_back(merge(literal, context).get_index());

    for (const auto literal : cond.template get_literals<DerivedTag>())
        if (literal.get_polarity())
            conj_cond.fluent_literals.push_back(merge<DerivedTag, OverlayRepository<Repository>, Repository, FluentTag>(literal, context).get_index());
};

/**
 * Assume each pre and eff is a set of literals
 *
 * Action = [pre, [<c1_pre,c1_eff>,...,cn_pre,cn_eff>]]
 * App_pre :- pre
 * App_c1_pre :- App_pre and ci_pre    forall i=1,...,n
 * e :- App_ci_pre                     forall i=1,...,n forall e in ci_eff
 */

static ::cista::offset::string create_applicability_name(View<Index<Action>, OverlayRepository<Repository>> action)
{
    return ::cista::offset::string { std::string { "@" } + action.get_name().str() };
}

static ::cista::offset::string create_triggered_name(View<Index<Action>, OverlayRepository<Repository>> action,
                                                     View<Index<ConditionalEffect>, OverlayRepository<Repository>> cond_eff)
{
    return ::cista::offset::string { std::string { "@" } + action.get_name().str() + std::string("_") + std::to_string(cond_eff.get_index().get_value()) };
}

static ::cista::offset::string create_applicability_name(View<Index<Axiom>, OverlayRepository<Repository>> axiom)
{
    return ::cista::offset::string { std::string { "@" } + axiom.get_head().get_predicate().get_name().str() + std::string("/")
                                     + std::to_string(axiom.get_head().get_predicate().get_arity()) };
}

static View<Index<Predicate<FluentTag>>, Repository> create_applicability_predicate(View<Index<Action>, OverlayRepository<Repository>> action,
                                                                                    MergeContext<OverlayRepository<Repository>, Repository>& context)
{
    auto predicate_ptr = context.builder.get_builder<Predicate<FluentTag>>();
    auto& predicate = *predicate_ptr;
    predicate.clear();

    predicate.name = create_applicability_name(action);
    predicate.arity = action.get_arity();

    canonicalize(predicate);
    return context.destination.get_or_create(predicate, context.builder.get_buffer()).first;
}

static View<Index<Atom<FluentTag>>, Repository> create_applicability_atom(View<Index<Action>, OverlayRepository<Repository>> action,
                                                                          MergeContext<OverlayRepository<Repository>, Repository>& context)
{
    auto atom_ptr = context.builder.get_builder<Atom<FluentTag>>();
    auto& atom = *atom_ptr;
    atom.clear();

    const auto applicability_predicate = create_applicability_predicate(action, context);

    atom.predicate = applicability_predicate.get_index();
    for (uint_t i = 0; i < applicability_predicate.get_arity(); ++i)
        atom.terms.push_back(Data<Term>(ParameterIndex(i)));

    canonicalize(atom);
    return context.destination.get_or_create(atom, context.builder.get_buffer()).first;
}

static View<Index<Literal<FluentTag>>, Repository> create_applicability_literal(View<Index<Action>, OverlayRepository<Repository>> action,
                                                                                MergeContext<OverlayRepository<Repository>, Repository>& context)
{
    auto literal_ptr = context.builder.get_builder<Literal<FluentTag>>();
    auto& literal = *literal_ptr;
    literal.clear();

    literal.polarity = true;
    literal.atom = create_applicability_atom(action, context).get_index();

    canonicalize(literal);
    return context.destination.get_or_create(literal, context.builder.get_buffer()).first;
}

static View<Index<Rule>, Repository> create_applicability_rule(View<Index<Action>, OverlayRepository<Repository>> action,
                                                               MergeContext<OverlayRepository<Repository>, Repository>& context)
{
    auto rule_ptr = context.builder.get_builder<Rule>();
    auto& rule = *rule_ptr;
    rule.clear();

    auto conj_cond_ptr = context.builder.get_builder<ConjunctiveCondition>();
    auto& conj_cond = *conj_cond_ptr;
    conj_cond.clear();

    for (const auto variable : action.get_variables())
        conj_cond.variables.push_back(merge(variable, context).get_index());
    append_from_condition(action.get_condition(), context, conj_cond);

    canonicalize(conj_cond);
    const auto new_conj_cond = context.destination.get_or_create(conj_cond, context.builder.get_buffer()).first;

    rule.variables = new_conj_cond.get_variables().get_data();
    rule.body = new_conj_cond.get_index();
    rule.head = create_applicability_atom(action, context).get_index();

    canonicalize(rule);
    return context.destination.get_or_create(rule, context.builder.get_buffer()).first;
}

static View<Index<Predicate<FluentTag>>, Repository> create_triggered_predicate(View<Index<Action>, OverlayRepository<Repository>> action,
                                                                                View<Index<ConditionalEffect>, OverlayRepository<Repository>> cond_eff,
                                                                                MergeContext<OverlayRepository<Repository>, Repository>& context)
{
    auto predicate_ptr = context.builder.get_builder<Predicate<FluentTag>>();
    auto& predicate = *predicate_ptr;
    predicate.clear();

    predicate.name = create_triggered_name(action, cond_eff);
    predicate.arity = action.get_arity() + cond_eff.get_arity();

    canonicalize(predicate);
    return context.destination.get_or_create(predicate, context.builder.get_buffer()).first;
}

static View<Index<Atom<FluentTag>>, Repository> create_triggered_atom(View<Index<Action>, OverlayRepository<Repository>> action,
                                                                      View<Index<ConditionalEffect>, OverlayRepository<Repository>> cond_eff,
                                                                      MergeContext<OverlayRepository<Repository>, Repository>& context)
{
    auto atom_ptr = context.builder.get_builder<Atom<FluentTag>>();
    auto& atom = *atom_ptr;
    atom.clear();

    const auto triggered_predicate = create_triggered_predicate(action, cond_eff, context);

    atom.predicate = triggered_predicate.get_index();
    for (uint_t i = 0; i < triggered_predicate.get_arity(); ++i)
        atom.terms.push_back(Data<Term>(ParameterIndex(i)));

    canonicalize(atom);
    return context.destination.get_or_create(atom, context.builder.get_buffer()).first;
}

static View<Index<Literal<FluentTag>>, Repository> create_triggered_literal(View<Index<Action>, OverlayRepository<Repository>> action,
                                                                            View<Index<ConditionalEffect>, OverlayRepository<Repository>> cond_eff,
                                                                            MergeContext<OverlayRepository<Repository>, Repository>& context)
{
    auto literal_ptr = context.builder.get_builder<Literal<FluentTag>>();
    auto& literal = *literal_ptr;
    literal.clear();

    literal.polarity = true;
    literal.atom = create_triggered_atom(action, cond_eff, context).get_index();

    canonicalize(literal);
    return context.destination.get_or_create(literal, context.builder.get_buffer()).first;
}

static View<Index<Rule>, Repository> create_triggered_rule(View<Index<Action>, OverlayRepository<Repository>> action,
                                                           View<Index<ConditionalEffect>, OverlayRepository<Repository>> cond_eff,
                                                           MergeContext<OverlayRepository<Repository>, Repository>& context)
{
    auto rule_ptr = context.builder.get_builder<Rule>();
    auto& rule = *rule_ptr;
    rule.clear();

    auto conj_cond_ptr = context.builder.get_builder<ConjunctiveCondition>();
    auto& conj_cond = *conj_cond_ptr;
    conj_cond.clear();

    for (const auto variable : action.get_variables())
        conj_cond.variables.push_back(merge(variable, context).get_index());
    for (const auto variable : cond_eff.get_variables())
        conj_cond.variables.push_back(merge(variable, context).get_index());
    append_from_condition(cond_eff.get_condition(), context, conj_cond);
    conj_cond.fluent_literals.push_back(create_applicability_literal(action, context).get_index());

    canonicalize(conj_cond);
    const auto new_conj_cond = context.destination.get_or_create(conj_cond, context.builder.get_buffer()).first;

    rule.variables = new_conj_cond.get_variables().get_data();
    rule.body = new_conj_cond.get_index();
    rule.head = create_triggered_atom(action, cond_eff, context).get_index();

    canonicalize(rule);
    return context.destination.get_or_create(rule, context.builder.get_buffer()).first;
}

static View<Index<Rule>, Repository> create_effect_rule(View<Index<Action>, OverlayRepository<Repository>> action,
                                                        View<Index<ConditionalEffect>, OverlayRepository<Repository>> cond_eff,
                                                        View<Index<Atom<FluentTag>>, Repository> effect,
                                                        MergeContext<OverlayRepository<Repository>, Repository>& context)
{
    auto rule_ptr = context.builder.get_builder<Rule>();
    auto& rule = *rule_ptr;
    rule.clear();

    auto conj_cond_ptr = context.builder.get_builder<ConjunctiveCondition>();
    auto& conj_cond = *conj_cond_ptr;
    conj_cond.clear();

    for (const auto variable : action.get_variables())
        conj_cond.variables.push_back(merge(variable, context).get_index());
    for (const auto variable : cond_eff.get_variables())
        conj_cond.variables.push_back(merge(variable, context).get_index());
    conj_cond.fluent_literals.push_back(create_triggered_literal(action, cond_eff, context).get_index());

    canonicalize(conj_cond);
    const auto new_conj_cond = context.destination.get_or_create(conj_cond, context.builder.get_buffer()).first;

    rule.variables = new_conj_cond.get_variables().get_data();
    rule.body = new_conj_cond.get_index();
    rule.head = effect.get_index();

    canonicalize(rule);
    return context.destination.get_or_create(rule, context.builder.get_buffer()).first;
}

static View<Index<Predicate<FluentTag>>, Repository> create_applicability_predicate(View<Index<Axiom>, OverlayRepository<Repository>> axiom,
                                                                                    MergeContext<OverlayRepository<Repository>, Repository>& context)
{
    auto predicate_ptr = context.builder.get_builder<Predicate<FluentTag>>();
    auto& predicate = *predicate_ptr;
    predicate.clear();

    predicate.name = create_applicability_name(axiom);
    predicate.arity = axiom.get_arity();

    canonicalize(predicate);
    return context.destination.get_or_create(predicate, context.builder.get_buffer()).first;
}

static View<Index<Atom<FluentTag>>, Repository> create_applicability_atom(View<Index<Axiom>, OverlayRepository<Repository>> axiom,
                                                                          MergeContext<OverlayRepository<Repository>, Repository>& context)
{
    auto atom_ptr = context.builder.get_builder<Atom<FluentTag>>();
    auto& atom = *atom_ptr;
    atom.clear();

    const auto applicability_predicate = create_applicability_predicate(axiom, context);

    atom.predicate = applicability_predicate.get_index();
    for (uint_t i = 0; i < applicability_predicate.get_arity(); ++i)
        atom.terms.push_back(Data<Term>(ParameterIndex(i)));

    canonicalize(atom);
    return context.destination.get_or_create(atom, context.builder.get_buffer()).first;
}

static View<Index<Literal<FluentTag>>, Repository> create_applicability_literal(View<Index<Axiom>, OverlayRepository<Repository>> axiom,
                                                                                MergeContext<OverlayRepository<Repository>, Repository>& context)
{
    auto literal_ptr = context.builder.get_builder<Literal<FluentTag>>();
    auto& literal = *literal_ptr;
    literal.clear();

    literal.polarity = true;
    literal.atom = create_applicability_atom(axiom, context).get_index();

    canonicalize(literal);
    return context.destination.get_or_create(literal, context.builder.get_buffer()).first;
}

static View<Index<Rule>, Repository> create_applicability_rule(View<Index<Axiom>, OverlayRepository<Repository>> axiom,
                                                               MergeContext<OverlayRepository<Repository>, Repository>& context)
{
    auto rule_ptr = context.builder.get_builder<Rule>();
    auto& rule = *rule_ptr;
    rule.clear();

    auto conj_cond_ptr = context.builder.get_builder<ConjunctiveCondition>();
    auto& conj_cond = *conj_cond_ptr;
    conj_cond.clear();

    for (const auto variable : axiom.get_variables())
        conj_cond.variables.push_back(merge(variable, context).get_index());
    append_from_condition(axiom.get_body(), context, conj_cond);

    canonicalize(conj_cond);
    const auto new_conj_cond = context.destination.get_or_create(conj_cond, context.builder.get_buffer()).first;

    rule.variables = new_conj_cond.get_variables().get_data();
    rule.body = new_conj_cond.get_index();
    rule.head = create_applicability_atom(axiom, context).get_index();

    canonicalize(rule);
    return context.destination.get_or_create(rule, context.builder.get_buffer()).first;
}

static View<Index<Rule>, Repository> create_effect_rule(View<Index<Axiom>, OverlayRepository<Repository>> axiom,
                                                        View<Index<Atom<FluentTag>>, Repository> effect,
                                                        MergeContext<OverlayRepository<Repository>, Repository>& context)
{
    auto rule_ptr = context.builder.get_builder<Rule>();
    auto& rule = *rule_ptr;
    rule.clear();

    auto conj_cond_ptr = context.builder.get_builder<ConjunctiveCondition>();
    auto& conj_cond = *conj_cond_ptr;
    conj_cond.clear();

    for (const auto variable : axiom.get_variables())
        conj_cond.variables.push_back(merge(variable, context).get_index());
    conj_cond.fluent_literals.push_back(create_applicability_literal(axiom, context).get_index());

    canonicalize(conj_cond);
    const auto new_conj_cond = context.destination.get_or_create(conj_cond, context.builder.get_buffer()).first;

    rule.variables = new_conj_cond.get_variables().get_data();
    rule.body = new_conj_cond.get_index();
    rule.head = effect.get_index();

    canonicalize(rule);
    return context.destination.get_or_create(rule, context.builder.get_buffer()).first;
}

static void translate_action_to_delete_free_rules(View<Index<Action>, OverlayRepository<Repository>> action,
                                                  Data<Program>& program,
                                                  MergeContext<OverlayRepository<Repository>, Repository>& context,
                                                  GroundTaskProgram::RuleToActionsMapping& rule_to_actions_mapping)
{
    program.fluent_predicates.push_back(create_applicability_predicate(action, context).get_index());

    const auto applicability_rule = create_applicability_rule(action, context);

    rule_to_actions_mapping[applicability_rule].emplace_back(action);

    program.rules.push_back(applicability_rule.get_index());

    for (const auto cond_eff : action.get_effects())
    {
        program.fluent_predicates.push_back(create_triggered_predicate(action, cond_eff, context).get_index());

        program.rules.push_back(create_triggered_rule(action, cond_eff, context).get_index());

        for (const auto literal : cond_eff.get_effect().get_literals())
        {
            if (!literal.get_polarity())
                continue;  /// ignore delete effects

            program.rules.push_back(create_effect_rule(action, cond_eff, merge(literal.get_atom(), context), context).get_index());
        }
    }
}

static void translate_axiom_to_delete_free_axiom_rules(View<Index<Axiom>, OverlayRepository<Repository>> axiom,
                                                       Data<Program>& program,
                                                       MergeContext<OverlayRepository<Repository>, Repository>& context,
                                                       GroundTaskProgram::RuleToAxiomsMapping& rule_to_axioms_mapping)
{
    program.fluent_predicates.push_back(create_applicability_predicate(axiom, context).get_index());

    const auto applicability_rule = create_applicability_rule(axiom, context);

    rule_to_axioms_mapping[applicability_rule].emplace_back(axiom);

    program.rules.push_back(applicability_rule.get_index());

    program.rules.push_back(
        create_effect_rule(axiom, merge<DerivedTag, OverlayRepository<Repository>, Repository, FluentTag>(axiom.get_head(), context), context).get_index());
}

static View<Index<Program>, Repository> create(const LiftedTask& task,
                                               GroundTaskProgram::RuleToActionsMapping& rule_to_actions_mapping,
                                               GroundTaskProgram::RuleToAxiomsMapping& rule_to_axioms_mapping,
                                               Repository& repository)
{
    auto merge_cache = MergeCache<OverlayRepository<Repository>, Repository>();
    auto builder = Builder();
    auto context = MergeContext<OverlayRepository<Repository>, Repository>(builder, repository, merge_cache);
    auto program_ptr = builder.get_builder<Program>();
    auto& program = *program_ptr;
    program.clear();

    for (const auto predicate : task.get_task().get_domain().get_predicates<StaticTag>())
        program.static_predicates.push_back(merge(predicate, context).get_index());

    for (const auto predicate : task.get_task().get_domain().get_predicates<FluentTag>())
        program.fluent_predicates.push_back(merge(predicate, context).get_index());

    for (const auto predicate : task.get_task().get_domain().get_predicates<DerivedTag>())
        program.fluent_predicates.push_back(merge<DerivedTag, OverlayRepository<Repository>, Repository, FluentTag>(predicate, context).get_index());

    for (const auto predicate : task.get_task().get_derived_predicates())
        program.fluent_predicates.push_back(merge<DerivedTag, OverlayRepository<Repository>, Repository, FluentTag>(predicate, context).get_index());

    for (const auto function : task.get_task().get_domain().get_functions<StaticTag>())
        program.static_functions.push_back(merge(function, context).get_index());

    for (const auto function : task.get_task().get_domain().get_functions<FluentTag>())
        program.fluent_functions.push_back(merge(function, context).get_index());

    // We can ignore auxiliary function total-cost because it never occurs in a condition

    for (const auto object : task.get_task().get_domain().get_constants())
        program.objects.push_back(merge(object, context).get_index());
    for (const auto object : task.get_task().get_objects())
        program.objects.push_back(merge(object, context).get_index());

    for (const auto atom : task.get_task().get_atoms<StaticTag>())
        program.static_atoms.push_back(merge(atom, context).get_index());

    for (const auto atom : task.get_task().get_atoms<FluentTag>())
        program.fluent_atoms.push_back(merge(atom, context).get_index());

    for (const auto fterm_value : task.get_task().get_fterm_values<StaticTag>())
        program.static_fterm_values.push_back(merge(fterm_value, context).get_index());

    for (const auto action : task.get_task().get_domain().get_actions())
        translate_action_to_delete_free_rules(action, program, context, rule_to_actions_mapping);

    for (const auto axiom : task.get_task().get_domain().get_axioms())
        translate_axiom_to_delete_free_axiom_rules(axiom, program, context, rule_to_axioms_mapping);

    for (const auto axiom : task.get_task().get_axioms())
        translate_axiom_to_delete_free_axiom_rules(axiom, program, context, rule_to_axioms_mapping);

    canonicalize(program);
    return repository.get_or_create(program, builder.get_buffer()).first;
}

GroundTaskProgram::GroundTaskProgram(const LiftedTask& task) :
    m_rule_to_actions(),
    m_rule_to_axioms(),
    m_repository(std::make_shared<Repository>()),
    m_program(create(task, m_rule_to_actions, m_rule_to_axioms, *m_repository)),
    m_domains(analysis::compute_variable_domains(m_program)),
    m_strata(analysis::compute_rule_stratification(m_program)),
    m_listeners(analysis::compute_listeners(m_strata))
{
}

const GroundTaskProgram::RuleToActionsMapping& GroundTaskProgram::get_rule_to_actions_mapping() const noexcept { return m_rule_to_actions; }

const GroundTaskProgram::RuleToAxiomsMapping& GroundTaskProgram::get_rule_to_axioms_mapping() const noexcept { return m_rule_to_axioms; }

View<Index<Program>, Repository> GroundTaskProgram::get_program() const noexcept { return m_program; }

const RepositoryPtr& GroundTaskProgram::get_repository() const noexcept { return m_repository; }

const analysis::ProgramVariableDomains& GroundTaskProgram::get_domains() const noexcept { return m_domains; }

const analysis::RuleStrata& GroundTaskProgram::get_strata() const noexcept { return m_strata; }

const analysis::ListenerStrata& GroundTaskProgram::get_listeners() const noexcept { return m_listeners; }

}
