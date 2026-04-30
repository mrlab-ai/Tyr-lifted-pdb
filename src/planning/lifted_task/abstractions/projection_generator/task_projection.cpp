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

#include "tyr/analysis/domains.hpp"
#include "tyr/common/block_array_set.hpp"
#include "tyr/common/declarations.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/common/itertools.hpp"
#include "tyr/common/onetbb.hpp"
#include "tyr/formalism/planning/builder.hpp"
#include "tyr/formalism/planning/datas.hpp"
#include "tyr/formalism/planning/declarations.hpp"
#include "tyr/formalism/planning/fdr_context.hpp"
#include "tyr/formalism/planning/formatter.hpp"
#include "tyr/formalism/planning/grounder.hpp"
#include "tyr/formalism/planning/merge.hpp"
#include "tyr/formalism/planning/mutable/mutable.hpp"
#include "tyr/formalism/planning/planning_task.hpp"
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/formalism/planning/variable_dependency_graph.hpp"
#include "tyr/formalism/planning/views.hpp"
#include "tyr/formalism/unification/unification.hpp"
#include "tyr/planning/abstractions/explicit_projection.hpp"
#include "tyr/planning/abstractions/pattern_generator.hpp"
#include "tyr/planning/abstractions/projection_generator.hpp"
#include "tyr/planning/applicability.hpp"
#include "tyr/planning/declarations.hpp"
#include "tyr/planning/formatter.hpp"
#include "tyr/planning/heuristics/blind.hpp"
#include "tyr/planning/lifted_task.hpp"
#include "tyr/planning/lifted_task/abstractions/projection_generator.hpp"
#include "tyr/planning/lifted_task/node.hpp"
#include "tyr/planning/lifted_task/state_view.hpp"
#include "tyr/planning/lifted_task/successor_generator.hpp"
#include "tyr/planning/lifted_task/unpacked_state.hpp"

namespace f = tyr::formalism;
namespace fp = tyr::formalism::planning;

namespace tyr::planning
{
namespace
{

// project_variables

void project_variables(fp::VariableListView elements,
                       uint_t parent_arity,
                       const Map<f::ParameterIndex, f::ParameterIndex>& mapping,
                       IndexList<f::Variable>& ref_projected_variables,
                       fp::MergeContext& context)
{
    ref_projected_variables.clear();

    for (uint_t p = parent_arity; p < parent_arity + elements.size(); ++p)
    {
        if (mapping.contains(f::ParameterIndex { p }))
        {
            assert(p - parent_arity < elements.size());
            ref_projected_variables.push_back(merge_p2p(elements[p - parent_arity], context).first.get_index());
        }
    }
}

// should_keep

bool should_keep(fp::TermView element, const Map<f::ParameterIndex, f::ParameterIndex>& mapping)
{
    return visit(
        [&](auto&& arg)
        {
            using Alternative = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<Alternative, f::ParameterIndex>)
                return mapping.contains(arg);
            else if constexpr (std::is_same_v<Alternative, fp::ObjectView>)
                return true;
            else
                static_assert(dependent_false<Alternative>::value, "Missing case");
        },
        element.get_variant());
}

bool should_keep(fp::TermListView element, const Map<f::ParameterIndex, f::ParameterIndex>& mapping)
{
    return std::all_of(element.begin(), element.end(), [&](const auto& arg) { return should_keep(arg, mapping); });
}

template<f::FactKind T>
bool should_keep(fp::AtomView<T> element, const Map<f::ParameterIndex, f::ParameterIndex>& mapping, const Pattern& pattern)
{
    return should_keep(element.get_terms(), mapping);
}

template<>
bool should_keep(fp::AtomView<f::FluentTag> element, const Map<f::ParameterIndex, f::ParameterIndex>& mapping, const Pattern& pattern)
{
    return pattern.predicates_set.contains(element.get_predicate()) && should_keep(element.get_terms(), mapping);
}

template<f::FactKind T>
bool should_keep(fp::LiteralView<T> element, const Map<f::ParameterIndex, f::ParameterIndex>& mapping, const Pattern& pattern)
{
    return should_keep(element.get_atom(), mapping, pattern);
}

template<f::FactKind T>
void append_projected_atom(fp::GroundAtomView<T> element, const Pattern& pattern, IndexList<fp::GroundAtom<T>>& ref_projected_atoms, fp::MergeContext& context)
{
    if (pattern.predicates_set.contains(element.get_predicate()))
        ref_projected_atoms.push_back(fp::merge_p2p(element, context).first.get_index());
}

template<f::FactKind T>
void append_projected_literal(fp::GroundLiteralView<T> element,
                              const Map<f::ParameterIndex, f::ParameterIndex>& mapping,
                              const Pattern& pattern,
                              IndexList<fp::GroundLiteral<T>>& ref_projected_literal,
                              fp::MergeContext& context)
{
    if (should_keep(element, mapping, pattern))
        ref_projected_literal.push_back(fp::merge_p2p(element, context).first.get_index());
}

// compute_variable_remapping

void compute_variable_remapping(fp::TermView element, Map<f::ParameterIndex, f::ParameterIndex>& result)
{
    visit(
        [&](auto&& arg)
        {
            using Alternative = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<Alternative, f::ParameterIndex>)
                result.try_emplace(arg, f::ParameterIndex { static_cast<uint_t>(result.size()) });
            else if constexpr (std::is_same_v<Alternative, fp::ObjectView>) {}
            else
                static_assert(dependent_false<Alternative>::value, "Missing case");
        },
        element.get_variant());
}

void compute_variable_remapping(fp::TermListView element, Map<f::ParameterIndex, f::ParameterIndex>& result)
{
    for (const auto term : element)
        compute_variable_remapping(term, result);
}

void compute_variable_remapping(fp::ConjunctiveConditionView element, const Pattern& pattern, Map<f::ParameterIndex, f::ParameterIndex>& result)
{
    for (const auto literal : element.get_literals<f::FluentTag>())
    {
        if (pattern.predicates_set.contains(literal.get_atom().get_predicate()))
        {
            compute_variable_remapping(literal.get_atom().get_terms(), result);
        }
    }
}

void compute_variable_remapping(fp::ConjunctiveEffectView element, const Pattern& pattern, Map<f::ParameterIndex, f::ParameterIndex>& result)
{
    for (const auto literal : element.get_literals())
    {
        if (pattern.predicates_set.contains(literal.get_atom().get_predicate()))
        {
            compute_variable_remapping(literal.get_atom().get_terms(), result);
        }
    }
}

Map<f::ParameterIndex, f::ParameterIndex> compute_variable_remapping(fp::ActionView element, const Pattern& pattern)
{
    auto result = Map<f::ParameterIndex, f::ParameterIndex> {};

    // Base case: keep all variable of atoms over predicates in the pattern.

    compute_variable_remapping(element.get_condition(), pattern, result);

    for (const auto effect : element.get_effects())
    {
        compute_variable_remapping(effect.get_condition(), pattern, result);
        compute_variable_remapping(effect.get_effect(), pattern, result);
    }

    return result;  // Overapproximation: skip connectedness.

    // Inductive case: keep all variable connected via a static dependency.

    auto vdg = fp::VariableDependencyGraph(element);

    auto queue = std::vector<f::ParameterIndex> {};
    auto visited = std::vector<bool>(element.get_arity(), false);
    for (const auto& [p, _] : result)
    {
        queue.push_back(p);
        visited[uint_t(p)] = true;
    }

    while (!queue.empty())
    {
        const auto pi = static_cast<uint_t>(queue.back());
        queue.pop_back();

        for (uint_t pj = 0; pj < vdg.k(); ++pj)
        {
            if (vdg.has_dependency<f::StaticTag>(pi, pj))
            {
                result.try_emplace(f::ParameterIndex { pj }, f::ParameterIndex { static_cast<uint_t>(result.size()) });
                if (!visited[uint_t(pj)])
                {
                    queue.push_back(f::ParameterIndex { pj });
                    visited[uint_t(pj)] = true;
                }
            }
        }
    }

    return result;
}

// remap_term

auto remap_term(fp::TermView element, const Map<f::ParameterIndex, f::ParameterIndex>& mapping)
{
    return visit(
        [&](auto&& arg) -> Data<f::Term>
        {
            using Alternative = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<Alternative, f::ParameterIndex>)
                return Data<f::Term> { mapping.at(arg) };
            else if constexpr (std::is_same_v<Alternative, fp::ObjectView>)
                return Data<f::Term> { arg.get_index() };
            else
                static_assert(dependent_false<Alternative>::value, "Missing case");
        },
        element.get_variant());
}

// merge_*

template<f::FactKind T>
auto merge_projected_atom(fp::AtomView<T> element, const Map<f::ParameterIndex, f::ParameterIndex>& mapping, fp::MergeContext& context)
{
    auto atom_ptr = context.builder.template get_builder<fp::Atom<T>>();
    auto& atom = *atom_ptr;
    atom.clear();

    atom.predicate = merge_p2p(element.get_predicate(), context).first.get_index();
    for (const auto term : element.get_terms())
        atom.terms.push_back(remap_term(term, mapping));

    canonicalize(atom);
    return context.destination.get_or_create(atom);
}

template<f::FactKind T>
auto merge_projected_literal(fp::LiteralView<T> element, const Map<f::ParameterIndex, f::ParameterIndex>& mapping, fp::MergeContext& context)
{
    auto literal_ptr = context.builder.template get_builder<fp::Literal<T>>();
    auto& literal = *literal_ptr;
    literal.clear();

    literal.polarity = element.get_polarity();
    literal.atom = merge_projected_atom(element.get_atom(), mapping, context).first.get_index();

    canonicalize(literal);
    return context.destination.get_or_create(literal);
}

// append_*

template<f::FactKind T>
void append_projected_literal(fp::LiteralView<T> element,
                              const Map<f::ParameterIndex, f::ParameterIndex>& mapping,
                              const Pattern& pattern,
                              IndexList<fp::Literal<T>>& ref_projected_literal,
                              fp::MergeContext& context)
{
    if (should_keep(element, mapping, pattern))
        ref_projected_literal.push_back(merge_projected_literal(element, mapping, context).first.get_index());
}

template<f::FactKind T>
void append_projected_fact(fp::FDRFactView<T> element,
                           const Pattern& pattern,
                           DataList<fp::FDRFact<T>>& ref_projected_facts,
                           fp::MergeContext& context,
                           fp::FDRContext& fdr_context)
{
    if (pattern.facts_set.contains(element))
    {
        const auto [atom, inserted] = fp::merge_p2p(*element.get_atom(), context);
        assert(!inserted);
        ref_projected_facts.push_back(fdr_context.get_fact(atom));
    }
}

// append_* / create_*

auto create_projected_goal(fp::GroundConjunctiveConditionView element, const Pattern& pattern, fp::MergeContext& context, fp::FDRContext& fdr_context)
{
    auto conj_cond_ptr = context.builder.template get_builder<fp::GroundConjunctiveCondition>();
    auto& conj_cond = *conj_cond_ptr;
    conj_cond.clear();

    for (const auto literal : element.template get_literals<f::StaticTag>())
        conj_cond.static_literals.push_back(merge_p2p(literal, context).first.get_index());  // always useful to have
    for (const auto fact : element.template get_facts<f::PositiveTag>())
        append_projected_fact(fact, pattern, conj_cond.positive_facts, context, fdr_context);
    for (const auto fact : element.template get_facts<f::NegativeTag>())
        append_projected_fact(fact, pattern, conj_cond.negative_facts, context, fdr_context);

    canonicalize(conj_cond);
    return context.destination.get_or_create(conj_cond);
}

auto create_projected_conjunctive_condition(fp::ConjunctiveConditionView element,
                                            uint_t parent_arity,
                                            fp::MergeContext& context,
                                            const Map<f::ParameterIndex, f::ParameterIndex>& mapping,
                                            const Pattern& pattern)
{
    auto conj_cond_ptr = context.builder.template get_builder<fp::ConjunctiveCondition>();
    auto& conj_cond = *conj_cond_ptr;
    conj_cond.clear();

    project_variables(element.get_variables(), parent_arity, mapping, conj_cond.variables, context);
    for (const auto literal : element.get_literals<f::StaticTag>())
        append_projected_literal(literal, mapping, pattern, conj_cond.static_literals, context);
    for (const auto literal : element.get_literals<f::FluentTag>())
        append_projected_literal(literal, mapping, pattern, conj_cond.fluent_literals, context);

    canonicalize(conj_cond);
    return context.destination.get_or_create(conj_cond);
}

auto create_projected_conjunctive_effect(fp::ConjunctiveEffectView element,
                                         fp::MergeContext& context,
                                         const Map<f::ParameterIndex, f::ParameterIndex>& mapping,
                                         const Pattern& pattern)
{
    auto conj_effect_ptr = context.builder.template get_builder<fp::ConjunctiveEffect>();
    auto& conj_eff = *conj_effect_ptr;
    conj_eff.clear();

    for (const auto literal : element.get_literals())
        append_projected_literal(literal, mapping, pattern, conj_eff.literals, context);

    canonicalize(conj_eff);
    return context.destination.get_or_create(conj_eff);
}

void append_projected_conditional_effect(fp::ConditionalEffectView element,
                                         uint_t parent_arity,
                                         fp::MergeContext& context,
                                         IndexList<fp::ConditionalEffect>& ref_projected_cond_effect,
                                         const Map<f::ParameterIndex, f::ParameterIndex>& mapping,
                                         const Pattern& pattern)
{
    auto cond_effect_ptr = context.builder.template get_builder<fp::ConditionalEffect>();
    auto& cond_effect = *cond_effect_ptr;
    cond_effect.clear();

    project_variables(element.get_variables(), parent_arity, mapping, cond_effect.variables, context);
    cond_effect.condition = create_projected_conjunctive_condition(element.get_condition(), parent_arity, context, mapping, pattern).first.get_index();
    cond_effect.effect = create_projected_conjunctive_effect(element.get_effect(), context, mapping, pattern).first.get_index();

    canonicalize(cond_effect);
    ref_projected_cond_effect.push_back(context.destination.get_or_create(cond_effect).first.get_index());
}

void append_projected_action(fp::ActionView element,
                             fp::MergeContext& context,
                             IndexList<fp::Action>& ref_projected_actions,
                             ProjectionMapping<LiftedTag>::ActionMapping& projected_to_original_action,
                             const Pattern& pattern)
{
    auto action_ptr = context.builder.template get_builder<fp::Action>();
    auto& action = *action_ptr;
    action.clear();

    auto variable_remapping = compute_variable_remapping(element, pattern);

    // Invert original -> projected to projected -> original so that transition
    // labels can later be lifted back to the original action parameter space.
    auto projected_to_original = std::vector<f::ParameterIndex>(variable_remapping.size());
    for (const auto& [original_p, projected_p] : variable_remapping)
        projected_to_original[uint_t(projected_p)] = original_p;

    // Some actions have similar names but different bodies. To avoid name clashes, we append the action index to the name. This is not a problem since the name
    // is only used for debugging purposes.
    action.name = std::string(element.get_name()) + "_" + to_string(element.get_index());
    project_variables(element.get_variables(), uint_t(0), variable_remapping, action.variables, context);
    action.original_arity = action.variables.size();
    action.condition = create_projected_conjunctive_condition(element.get_condition(), uint_t(0), context, variable_remapping, pattern).first.get_index();
    for (const auto effect : element.get_effects())
        append_projected_conditional_effect(effect, element.get_arity(), context, action.effects, variable_remapping, pattern);

    canonicalize(action);
    const auto new_action = context.destination.get_or_create(action).first;

    projected_to_original_action.emplace(new_action, typename ProjectionMapping<LiftedTag>::ProjectedActionInfo { element, std::move(projected_to_original) });
    ref_projected_actions.push_back(new_action.get_index());
}

auto create_projected_formalism_domain(fp::DomainView element,
                                       std::shared_ptr<fp::Repository> destination,
                                       fp::MergeContext& context,
                                       fp::RepositoryFactoryPtr factory,
                                       ProjectionMapping<LiftedTag>::ActionMapping& projected_to_original_action,
                                       const Pattern& pattern)
{
    auto domain_ptr = context.builder.template get_builder<fp::Domain>();
    auto& domain = *domain_ptr;
    domain.clear();

    domain.name = element.get_name();
    for (const auto predicate : element.get_predicates<f::StaticTag>())
        domain.static_predicates.push_back(fp::merge_p2p(predicate, context).first.get_index());
    for (const auto predicate : element.get_predicates<f::FluentTag>())
        domain.fluent_predicates.push_back(fp::merge_p2p(predicate, context).first.get_index());
    for (const auto object : element.get_constants())
        domain.constants.push_back(fp::merge_p2p(object, context).first.get_index());
    for (const auto action : element.get_actions())
        append_projected_action(action, context, domain.actions, projected_to_original_action, pattern);

    canonicalize(domain);
    return fp::PlanningDomain(context.destination.get_or_create(domain).first, std::move(destination), std::move(factory));
}

auto create_projected_formalism_task(const fp::PlanningTask& planning_task,
                                     const Pattern& pattern,
                                     fp::RepositoryPtr destination,
                                     fp::RepositoryFactoryPtr factory,
                                     fp::FDRContextPtr fdr_context)
{
    auto projected_to_original_action = ProjectionMapping<LiftedTag>::ActionMapping {};
    auto builder = fp::Builder();

    auto context = fp::MergeContext(builder, *destination);

    const auto project_domain =
        create_projected_formalism_domain(planning_task.get_domain().get_domain(), destination, context, factory, projected_to_original_action, pattern);

    auto task_ptr = builder.get_builder<fp::Task>();
    auto& task = *task_ptr;
    task.clear();

    task.name = planning_task.get_task().get_name();
    task.domain = project_domain.get_domain().get_index();
    for (const auto predicate : planning_task.get_task().get_derived_predicates())
        task.derived_predicates.push_back(fp::merge_p2p(predicate, context).first.get_index());
    for (const auto object : planning_task.get_task().get_objects())
        task.objects.push_back(fp::merge_p2p(object, context).first.get_index());
    for (const auto atom : planning_task.get_task().get_atoms<f::StaticTag>())
        task.static_atoms.push_back(fp::merge_p2p(atom, context).first.get_index());  // always useful to have
    for (const auto atom : planning_task.get_task().get_atoms<f::FluentTag>())
        append_projected_atom(atom, pattern, task.fluent_atoms, context);
    task.goal = create_projected_goal(planning_task.get_task().get_goal(), pattern, context, *fdr_context).first.get_index();

    canonicalize(task);
    return std::make_pair(fp::PlanningTask(destination->get_or_create(task).first, std::move(fdr_context), destination, project_domain),
                          std::move(projected_to_original_action));
}

}

std::pair<LiftedTaskPtr, ProjectionMapping<LiftedTag>::ActionMapping> project_task(const Task<LiftedTag>& original_task, const Pattern& pattern)
{
    // Create a new repository for the projected task.
    const auto& factory = original_task.get_domain().get_repository_factory();
    auto destination = factory->create_shared(original_task.get_repository().get());

    // Copy the FDRContext although we never modify it. Just in case we decide to do so.
    auto builder = fp::Builder();
    auto fdr_context = std::make_shared<fp::FDRContext>(*original_task.get_fdr_context(), builder, destination);

    // Project the original task.
    auto [projected_formalism_task, projected_to_original_action] =
        create_projected_formalism_task(original_task.get_formalism_task(), pattern, destination, factory, fdr_context);

    return std::make_pair(LiftedTask::create(projected_formalism_task), std::move(projected_to_original_action));
}
}
