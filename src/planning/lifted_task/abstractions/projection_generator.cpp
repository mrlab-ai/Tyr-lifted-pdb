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

#include "tyr/analysis/domains.hpp"
#include "tyr/common/block_array_set.hpp"
#include "tyr/common/declarations.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/common/itertools.hpp"
#include "tyr/common/less.hpp"
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
#include "tyr/planning/lifted_task/node.hpp"
#include "tyr/planning/lifted_task/state_view.hpp"
#include "tyr/planning/lifted_task/successor_generator.hpp"
#include "tyr/planning/lifted_task/unpacked_state.hpp"

namespace f = tyr::formalism;
namespace fp = tyr::formalism::planning;
namespace u = tyr::formalism::unification;

namespace tyr::planning
{
namespace
{
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
bool should_keep(fp::AtomView<T> element, const Map<f::ParameterIndex, f::ParameterIndex>& mapping)
{
    return should_keep(element.get_terms(), mapping);
}

template<f::FactKind T>
bool should_keep(fp::LiteralView<T> element, const Map<f::ParameterIndex, f::ParameterIndex>& mapping)
{
    return should_keep(element.get_atom(), mapping);
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
                              IndexList<fp::GroundLiteral<T>>& ref_projected_literal,
                              fp::MergeContext& context)
{
    if (should_keep(element, mapping))
        ref_projected_literal.push_back(fp::merge_p2p(element, context).first.get_index());
}

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

template<f::FactKind T>
void append_projected_literal(fp::LiteralView<T> element,
                              const Map<f::ParameterIndex, f::ParameterIndex>& mapping,
                              IndexList<fp::Literal<T>>& ref_projected_literal,
                              fp::MergeContext& context)
{
    if (should_keep(element, mapping))
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
                                            const Map<f::ParameterIndex, f::ParameterIndex>& mapping)
{
    auto conj_cond_ptr = context.builder.template get_builder<fp::ConjunctiveCondition>();
    auto& conj_cond = *conj_cond_ptr;
    conj_cond.clear();

    project_variables(element.get_variables(), parent_arity, mapping, conj_cond.variables, context);
    for (const auto literal : element.get_literals<f::StaticTag>())
        append_projected_literal(literal, mapping, conj_cond.static_literals, context);
    for (const auto literal : element.get_literals<f::FluentTag>())
        append_projected_literal(literal, mapping, conj_cond.fluent_literals, context);

    canonicalize(conj_cond);
    return context.destination.get_or_create(conj_cond);
}

auto create_projected_conjunctive_effect(fp::ConjunctiveEffectView element, fp::MergeContext& context, const Map<f::ParameterIndex, f::ParameterIndex>& mapping)
{
    auto conj_effect_ptr = context.builder.template get_builder<fp::ConjunctiveEffect>();
    auto& conj_eff = *conj_effect_ptr;
    conj_eff.clear();

    for (const auto literal : element.get_literals())
        append_projected_literal(literal, mapping, conj_eff.literals, context);

    canonicalize(conj_eff);
    return context.destination.get_or_create(conj_eff);
}

void append_projected_conditional_effect(fp::ConditionalEffectView element,
                                         uint_t parent_arity,
                                         fp::MergeContext& context,
                                         IndexList<fp::ConditionalEffect>& ref_projected_cond_effect,
                                         const Map<f::ParameterIndex, f::ParameterIndex>& mapping)
{
    auto cond_effect_ptr = context.builder.template get_builder<fp::ConditionalEffect>();
    auto& cond_effect = *cond_effect_ptr;
    cond_effect.clear();

    project_variables(element.get_variables(), parent_arity, mapping, cond_effect.variables, context);
    cond_effect.condition = create_projected_conjunctive_condition(element.get_condition(), parent_arity, context, mapping).first.get_index();
    cond_effect.effect = create_projected_conjunctive_effect(element.get_effect(), context, mapping).first.get_index();

    canonicalize(cond_effect);
    ref_projected_cond_effect.push_back(context.destination.get_or_create(cond_effect).first.get_index());
}

/**
 * compute_variable_remapping
 */

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

/**
 * append_projected_action
 */

void append_projected_action(fp::ActionView element,
                             fp::MergeContext& context,
                             IndexList<fp::Action>& ref_projected_actions,
                             ProjectionMapping<LiftedTag>::ActionMapping& projected_to_original_action,
                             const Pattern& pattern)
{
    auto action_ptr = context.builder.template get_builder<fp::Action>();
    auto& action = *action_ptr;
    action.clear();

    // TODO: decide which variables to keep.

    auto variable_remapping = compute_variable_remapping(element, pattern);

    // std::cout << pattern << std::endl;

    // std::cout << element << std::endl;

    // std::cout << variable_remapping << std::endl;

    action.name = element.get_name();
    project_variables(element.get_variables(), uint_t(0), variable_remapping, action.variables, context);
    action.original_arity = action.variables.size();
    action.condition = create_projected_conjunctive_condition(element.get_condition(), uint_t(0), context, variable_remapping).first.get_index();
    for (const auto effect : element.get_effects())
        append_projected_conditional_effect(effect, element.get_arity(), context, action.effects, variable_remapping);

    canonicalize(action);
    const auto new_action = context.destination.get_or_create(action).first;
    projected_to_original_action.emplace(new_action, element);

    // std::cout << new_action << std::endl;

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
    {
        append_projected_action(action, context, domain.actions, projected_to_original_action, pattern);
    }

    canonicalize(domain);
    return fp::PlanningDomain(context.destination.get_or_create(domain).first, std::move(destination), std::move(factory));
}

/// @brief Project the task to the given pattern.
/// An atom is projected away iff the pattern does not mention the predicate
/// A (ground) fact is projected away iff the pattern deos not contain it.
///
/// The FDR mappings may diverge
/// @param planning_task
/// @param pattern
/// @return
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

/// @brief Create all 2^|pattern| abstract states.
/// This ignores reachability but suffices for domains without unsolvable states.
auto create_abstract_states(const Pattern& pattern, Task<LiftedTag>& task, StateRepository<LiftedTag>& state_repository)
{
    auto facts = std::vector<fp::FDRFactView<f::FluentTag>>(pattern.facts.begin(), pattern.facts.end());
    auto astates = std::vector<StateView<LiftedTag>> {};
    auto goal_vertices = std::vector<uint_t> {};

    {
        itertools::for_each_boolean_vector(
            [&](auto&& vec)
            {
                auto uastate = state_repository.get_unregistered_state();
                uastate->clear();

                for (uint_t i = 0; i < vec.size(); ++i)
                    if (vec[i])
                        uastate->set(facts[i].get_data());

                const auto astate = state_repository.register_state(uastate);

                const auto state_context = StateContext { task, astate.get_unpacked_state(), float_t { 0 } };
                const auto is_goal = is_dynamically_applicable(task.get_task().get_goal(), state_context);

                if (is_goal)
                    goal_vertices.push_back(uint_t(astate.get_index()));

                astates.push_back(astate);
            },
            pattern.size());
    }

    return std::make_pair(std::move(astates), std::move(goal_vertices));
}

bool is_ground(const Data<f::Term>& term) { return u::is_object(term); }

template<f::FactKind T>
bool is_ground(const fp::MutableAtom<T>& atom)
{
    return std::all_of(atom.terms.begin(), atom.terms.end(), [](const auto& term) { return is_ground(term); });
}

template<f::FactKind T>
bool contains_atom(const std::vector<fp::MutableAtom<T>>& atoms, const fp::MutableAtom<T>& atom)
{
    return std::find(atoms.begin(), atoms.end(), atom) != atoms.end();
}

std::vector<fp::MutableAtom<f::FluentTag>> difference_atoms(const std::vector<fp::MutableAtom<f::FluentTag>>& lhs,
                                                            const std::vector<fp::MutableAtom<f::FluentTag>>& rhs)
{
    auto result = std::vector<fp::MutableAtom<f::FluentTag>> {};
    for (const auto& atom : lhs)
    {
        if (!contains_atom(rhs, atom))
            result.push_back(atom);
    }
    return result;
}

size_t compute_sigma_domain_size(const fp::MutableAction& action)
{
    size_t domain_size = action.num_variables;

    for (const auto& ceff : action.effects)
        domain_size = std::max(domain_size, ceff.num_parent_variables + ceff.num_variables);

    return domain_size;
}

u::SubstitutionFunction<Data<f::Term>> make_sigma(const fp::MutableAction& action)
{
    return u::SubstitutionFunction<Data<f::Term>>::from_range(f::ParameterIndex(0), compute_sigma_domain_size(action));
}

std::optional<u::SubstitutionFunction<Index<f::Object>>> to_object_substitution(const u::SubstitutionFunction<Data<f::Term>>& sigma,
                                                                                size_t num_action_parameters)
{
    auto result = u::SubstitutionFunction<Index<f::Object>>::from_range(f::ParameterIndex(0), num_action_parameters);

    for (size_t i = 0; i < num_action_parameters; ++i)
    {
        const auto parameter = f::ParameterIndex(uint_t(i));

        if (!sigma.contains_parameter(parameter) || !sigma.is_bound(parameter))
            continue;

        const auto& term = *sigma[parameter];
        if (!u::is_object(term))
            return std::nullopt;

        [[maybe_unused]] const auto inserted = result.assign(parameter, u::get_object(term));
        assert(inserted);
    }

    return result;
}

std::vector<fp::MutableAtom<f::StaticTag>> collect_projected_static_atoms(const Task<LiftedTag>& task)
{
    auto result = std::vector<fp::MutableAtom<f::StaticTag>> {};
    result.reserve(task.get_task().get_atoms<f::StaticTag>().size());

    for (const auto atom : task.get_task().get_atoms<f::StaticTag>())
        result.emplace_back(atom);

    return result;
}

std::vector<fp::MutableAtom<f::FluentTag>> collect_visible_fluent_atoms(const StateView<LiftedTag>& state, const Pattern& pattern)
{
    auto result = std::vector<fp::MutableAtom<f::FluentTag>> {};
    result.reserve(pattern.facts_set.size());

    for (const auto fact : pattern.facts_set)
    {
        if (!state.get_unpacked_state().get(fact.get_variable()).is_none())
            result.emplace_back(fact.get_atom().value());
    }

    return result;
}

std::vector<fp::MutableAtom<f::FluentTag>> collect_pattern_atoms(const Pattern& pattern)
{
    auto result = std::vector<fp::MutableAtom<f::FluentTag>> {};
    result.reserve(pattern.atoms_set.size());

    for (const auto atom : pattern.atoms_set)
        result.emplace_back(atom);

    return result;
}

bool is_visible_atom(const fp::MutableAtom<f::FluentTag>& atom, const std::vector<fp::MutableAtom<f::FluentTag>>& visible_atoms)
{
    return contains_atom(visible_atoms, atom);
}

template<f::FactKind T>
bool literal_holds(const fp::MutableLiteral<T>& lit, const std::vector<fp::MutableAtom<T>>& atoms)
{
    assert(is_ground(lit.atom));

    if (lit.polarity)
        return contains_atom(atoms, lit.atom);

    return !contains_atom(atoms, lit.atom);
}

template<f::FactKind T>
std::optional<u::SubstitutionFunction<Data<f::Term>>>
match_literal_to_atom(const fp::MutableLiteral<T>& lit, const fp::MutableAtom<T>& atom, u::SubstitutionFunction<Data<f::Term>> sigma)
{
    const auto target_lit = fp::MutableLiteral<T>(atom, lit.polarity);
    return u::match(lit, target_lit, std::move(sigma));
}

bool same_atom_set(const std::vector<fp::MutableAtom<f::FluentTag>>& lhs, const std::vector<fp::MutableAtom<f::FluentTag>>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    return std::all_of(lhs.begin(), lhs.end(), [&](const auto& atom) { return contains_atom(rhs, atom); });
}

void push_unique(std::vector<fp::MutableAtom<f::FluentTag>>& atoms, const fp::MutableAtom<f::FluentTag>& atom)
{
    if (!contains_atom(atoms, atom))
        atoms.push_back(atom);
}

/**
 * Static literal satisfaction
 *
 * Enumerates all substitutions extending `sigma` that satisfy the static literals.
 */
template<typename Callback>
void satisfy_static_literals_rec(const fp::MutableLiteralList<f::StaticTag>& static_literals,
                                 size_t pos,
                                 const std::vector<fp::MutableAtom<f::StaticTag>>& static_atoms,
                                 const u::SubstitutionFunction<Data<f::Term>>& sigma,
                                 Callback&& callback)
{
    if (pos == static_literals.size())
    {
        callback(sigma);
        return;
    }

    const auto lit = u::apply_substitution_fixpoint(static_literals[pos], sigma);

    if (is_ground(lit.atom))
    {
        if (literal_holds(lit, static_atoms))
            satisfy_static_literals_rec(static_literals, pos + 1, static_atoms, sigma, std::forward<Callback>(callback));

        return;
    }

    // Negative nonground static literals are not handled existentially here.
    if (!lit.polarity)
        return;

    for (const auto& atom : static_atoms)
    {
        auto sigma2 = sigma;
        const auto matched = match_literal_to_atom(lit, atom, std::move(sigma2));
        if (!matched)
            continue;

        satisfy_static_literals_rec(static_literals, pos + 1, static_atoms, *matched, std::forward<Callback>(callback));
    }
}

/**
 * Visible fluent source condition check
 *
 * Only pattern-visible fluent atoms are checked exactly against the source abstract state.
 * Hidden fluent literals remain existential and are ignored here.
 */
bool visible_fluent_literals_hold_in_src(const fp::MutableLiteralList<f::FluentTag>& fluent_literals,
                                         const std::vector<fp::MutableAtom<f::FluentTag>>& src_atoms,
                                         const std::vector<fp::MutableAtom<f::FluentTag>>& visible_pattern_atoms,
                                         const u::SubstitutionFunction<Data<f::Term>>& sigma)
{
    for (const auto& lit0 : fluent_literals)
    {
        const auto lit = u::apply_substitution_fixpoint(lit0, sigma);

        if (!is_ground(lit.atom))
            continue;  // hidden existential support

        if (!is_visible_atom(lit.atom, visible_pattern_atoms))
            continue;  // not part of the abstraction

        if (!literal_holds(lit, src_atoms))
            return false;
    }

    return true;
}

/**
 * Enumerate all substitutions extending `sigma` that satisfy a conjunctive condition:
 * - static literals are matched exactly/existentially against projected static atoms
 * - visible fluent literals are checked exactly against the source abstract state
 * - hidden fluent literals are ignored here
 */
template<typename Callback>
void satisfy_condition_bindings(const fp::MutableConjunctiveCondition& condition,
                                const std::vector<fp::MutableAtom<f::StaticTag>>& static_atoms,
                                const std::vector<fp::MutableAtom<f::FluentTag>>& src_atoms,
                                const std::vector<fp::MutableAtom<f::FluentTag>>& visible_pattern_atoms,
                                const u::SubstitutionFunction<Data<f::Term>>& sigma,
                                Callback&& callback)
{
    satisfy_static_literals_rec(condition.static_literals,
                                0,
                                static_atoms,
                                sigma,
                                [&](const u::SubstitutionFunction<Data<f::Term>>& sigma2)
                                {
                                    if (!visible_fluent_literals_hold_in_src(condition.fluent_literals, src_atoms, visible_pattern_atoms, sigma2))
                                        return;

                                    callback(sigma2);
                                });
}

std::optional<u::SubstitutionFunction<Data<f::Term>>> match_effect_literal(const fp::MutableLiteral<f::FluentTag>& effect_lit,
                                                                           const fp::MutableAtom<f::FluentTag>& target_atom,
                                                                           bool target_polarity,
                                                                           u::SubstitutionFunction<Data<f::Term>> sigma)
{
    const auto target_lit = fp::MutableLiteral<f::FluentTag>(target_atom, target_polarity);
    return u::match(effect_lit, target_lit, std::move(sigma));
}

/**
 * Enumerate all effect-driven partial substitutions that explain the added/deleted visible atoms.
 */
template<typename Callback>
void unify_changes_rec(const fp::MutableAction& action,
                       const std::vector<fp::MutableAtom<f::FluentTag>>& added,
                       const std::vector<fp::MutableAtom<f::FluentTag>>& deleted,
                       size_t add_pos,
                       size_t del_pos,
                       const u::SubstitutionFunction<Data<f::Term>>& sigma,
                       Callback&& callback)
{
    if (add_pos == added.size() && del_pos == deleted.size())
    {
        callback(sigma);
        return;
    }

    if (add_pos < added.size())
    {
        const auto& target = added[add_pos];

        for (const auto& ceff : action.effects)
        {
            for (const auto& lit : ceff.effect.literals)
            {
                if (!lit.polarity)
                    continue;

                auto sigma2 = sigma;
                const auto matched = match_effect_literal(lit, target, true, std::move(sigma2));
                if (!matched)
                    continue;

                unify_changes_rec(action, added, deleted, add_pos + 1, del_pos, *matched, std::forward<Callback>(callback));
            }
        }

        return;
    }

    const auto& target = deleted[del_pos];

    for (const auto& ceff : action.effects)
    {
        for (const auto& lit : ceff.effect.literals)
        {
            if (lit.polarity)
                continue;

            auto sigma2 = sigma;
            const auto matched = match_effect_literal(lit, target, false, std::move(sigma2));
            if (!matched)
                continue;

            unify_changes_rec(action, added, deleted, add_pos, del_pos + 1, *matched, std::forward<Callback>(callback));
        }
    }
}

/**
 * Enumerate all completed substitutions whose visible firing effects explain exactly the abstract difference.
 *
 * We first satisfy the action condition, then recursively consider all conditional effects.
 * Effects that do not contribute visible pattern atoms may be skipped.
 */
template<typename Callback>
void enumerate_verified_bindings_rec(const fp::MutableAction& action,
                                     size_t effect_pos,
                                     const std::vector<fp::MutableAtom<f::FluentTag>>& src_atoms,
                                     const std::vector<fp::MutableAtom<f::FluentTag>>& dst_atoms,
                                     const std::vector<fp::MutableAtom<f::FluentTag>>& added,
                                     const std::vector<fp::MutableAtom<f::FluentTag>>& deleted,
                                     const std::vector<fp::MutableAtom<f::FluentTag>>& visible_pattern_atoms,
                                     const std::vector<fp::MutableAtom<f::StaticTag>>& static_atoms,
                                     const u::SubstitutionFunction<Data<f::Term>>& sigma,
                                     const std::vector<fp::MutableAtom<f::FluentTag>>& produced_add,
                                     const std::vector<fp::MutableAtom<f::FluentTag>>& produced_del,
                                     Callback&& callback)
{
    if (effect_pos == action.effects.size())
    {
        if (!same_atom_set(produced_add, added))
            return;

        if (!same_atom_set(produced_del, deleted))
            return;

        if (produced_add.empty() && produced_del.empty())
            return;

        callback(sigma);
        return;
    }

    const auto& ceff = action.effects[effect_pos];

    // Option 1: skip this effect (either it does not fire, or it only has hidden consequences).
    enumerate_verified_bindings_rec(action,
                                    effect_pos + 1,
                                    src_atoms,
                                    dst_atoms,
                                    added,
                                    deleted,
                                    visible_pattern_atoms,
                                    static_atoms,
                                    sigma,
                                    produced_add,
                                    produced_del,
                                    std::forward<Callback>(callback));

    // Option 2: satisfy the effect condition and realize its visible consequences.
    satisfy_condition_bindings(ceff.condition,
                               static_atoms,
                               src_atoms,
                               visible_pattern_atoms,
                               sigma,
                               [&](const u::SubstitutionFunction<Data<f::Term>>& sigma_eff)
                               {
                                   const auto grounded_ceff = u::apply_substitution_fixpoint(ceff, sigma_eff);

                                   auto produced_add2 = produced_add;
                                   auto produced_del2 = produced_del;

                                   for (const auto& lit : grounded_ceff.effect.literals)
                                   {
                                       if (!is_ground(lit.atom))
                                           return;

                                       if (!is_visible_atom(lit.atom, visible_pattern_atoms))
                                           continue;

                                       if (lit.polarity)
                                       {
                                           if (!contains_atom(dst_atoms, lit.atom))
                                               return;

                                           push_unique(produced_add2, lit.atom);
                                       }
                                       else
                                       {
                                           if (contains_atom(dst_atoms, lit.atom))
                                               return;

                                           push_unique(produced_del2, lit.atom);
                                       }
                                   }

                                   enumerate_verified_bindings_rec(action,
                                                                   effect_pos + 1,
                                                                   src_atoms,
                                                                   dst_atoms,
                                                                   added,
                                                                   deleted,
                                                                   visible_pattern_atoms,
                                                                   static_atoms,
                                                                   sigma_eff,
                                                                   produced_add2,
                                                                   produced_del2,
                                                                   callback);
                               });
}

/**
 * Enumerate all object substitutions for `action` that realize `src -> dst` over the pattern.
 */
template<typename Callback>
void for_each_unifier(fp::ActionView action,
                      const StateView<LiftedTag>& src,
                      const StateView<LiftedTag>& dst,
                      const Task<LiftedTag>& task,
                      const Pattern& pattern,
                      Callback&& callback)
{
    const auto mutable_action = fp::MutableAction(action);

    // Pattern-visible difference only.
    const auto src_atoms = collect_visible_fluent_atoms(src, pattern);
    const auto dst_atoms = collect_visible_fluent_atoms(dst, pattern);
    const auto added = difference_atoms(dst_atoms, src_atoms);
    const auto deleted = difference_atoms(src_atoms, dst_atoms);

    const auto visible_pattern_atoms = collect_pattern_atoms(pattern);
    const auto static_atoms = collect_projected_static_atoms(task);

    auto seen = std::vector<u::SubstitutionFunction<Index<f::Object>>> {};

    unify_changes_rec(mutable_action,
                      added,
                      deleted,
                      0,
                      0,
                      make_sigma(mutable_action),
                      [&](const u::SubstitutionFunction<Data<f::Term>>& sigma0)
                      {
                          satisfy_condition_bindings(mutable_action.condition,
                                                     static_atoms,
                                                     src_atoms,
                                                     visible_pattern_atoms,
                                                     sigma0,
                                                     [&](const u::SubstitutionFunction<Data<f::Term>>& sigma1)
                                                     {
                                                         enumerate_verified_bindings_rec(mutable_action,
                                                                                         0,
                                                                                         src_atoms,
                                                                                         dst_atoms,
                                                                                         added,
                                                                                         deleted,
                                                                                         visible_pattern_atoms,
                                                                                         static_atoms,
                                                                                         sigma1,
                                                                                         {},
                                                                                         {},
                                                                                         [&](const u::SubstitutionFunction<Data<f::Term>>& sigma_final)
                                                                                         {
                                                                                             const auto obj_sigma =
                                                                                                 to_object_substitution(sigma_final, action.get_arity());
                                                                                             if (!obj_sigma)
                                                                                                 return;

                                                                                             if (std::find(seen.begin(), seen.end(), *obj_sigma) != seen.end())
                                                                                                 return;

                                                                                             seen.push_back(*obj_sigma);
                                                                                             callback(*obj_sigma);
                                                                                         });
                                                     });
                      });
}

auto create_abstract_state_changing_transitions(const std::vector<StateView<LiftedTag>>& astates,
                                                const Pattern& pattern,
                                                const ProjectionMapping<LiftedTag>::ActionMapping& projected_to_original_action,
                                                Task<LiftedTag>& task,
                                                SuccessorGenerator<LiftedTag>& successor_generator,
                                                fp::FDRContext& fdr_context)
{
    auto labeled_succ_nodes = std::vector<LabeledNode<LiftedTag>> {};
    auto transitions = TransitionList {};
    auto adj_lists = std::vector<std::vector<uint_t>>(astates.size());

    for (size_t i = 0; i < astates.size(); ++i)
    {
        const auto& astate_i = astates[i];

        for (size_t j = 0; j < astates.size(); ++j)
        {
            if (i == j)
                continue;

            const auto& astate_j = astates[j];

            for (const auto& [projected_action, original_action] : projected_to_original_action)
            {
                for_each_unifier(projected_action,
                                 astate_i,
                                 astate_j,
                                 task,
                                 pattern,
                                 [&](const u::SubstitutionFunction<Index<f::Object>>& sigma)
                                 {
                                     const auto t = uint_t(transitions.size());
                                     const auto src = uint_t(astate_i.get_index());
                                     const auto dst = uint_t(astate_j.get_index());

                                     transitions.emplace_back(projected_action, sigma, src, dst);
                                     adj_lists[src].push_back(t);
                                 });
            }
        }
    }

    return std::make_pair(std::move(transitions), std::move(adj_lists));
}

auto create_projection(const Pattern& pattern, const Task<LiftedTag>& original_task)
{
    // Note: All projections share the same repository.
    const auto& factory = original_task.get_domain().get_repository_factory();
    auto destination = factory->create_shared(original_task.get_repository().get());

    // Note: Copy the FDRContext to avoid polluting the parent tasks FDRContext with non-existing atoms.
    auto builder = fp::Builder();
    auto fdr_context = std::make_shared<fp::FDRContext>(*original_task.get_fdr_context(), builder, destination);

    auto [projected_formalism_task, projected_to_original_action] =
        create_projected_formalism_task(original_task.get_formalism_task(), pattern, destination, factory, fdr_context);

    // std::cout << projected_formalism_task.get_task().get_domain() << std::endl;
    // std::cout << projected_formalism_task.get_task() << std::endl;

    auto projected_task = LiftedTask::create(projected_formalism_task);

    // Note: Each projection has its own StateRepository.
    auto execution_context = ExecutionContext::create(1);
    auto successor_generator = SuccessorGenerator<LiftedTag>(projected_task, execution_context);
    auto state_repository = successor_generator.get_state_repository();

    auto [astates, goal_vertices] = create_abstract_states(pattern, *projected_task, *state_repository);
    auto [transitions, adj_lists] =
        create_abstract_state_changing_transitions(astates, pattern, projected_to_original_action, *projected_task, successor_generator, *fdr_context);

    auto result = ProjectionAbstraction(
        std::make_shared<const ForwardProjectionAbstraction<LiftedTag>>(ProjectionMapping<LiftedTag>(pattern, projected_to_original_action),
                                                                        std::move(state_repository),
                                                                        std::move(astates),
                                                                        std::move(transitions),
                                                                        std::move(adj_lists),
                                                                        std::move(goal_vertices)));

    std::cout << result << std::endl;

    return result;
}
}

ProjectionGenerator<LiftedTag>::ProjectionGenerator(std::shared_ptr<const Task<LiftedTag>> task, PatternCollection patterns) :
    m_task(std::move(task)),
    m_patterns(std::move(patterns))
{
}

ProjectionAbstractionList<LiftedTag> ProjectionGenerator<LiftedTag>::generate()
{
    auto projections = ProjectionAbstractionList<LiftedTag> {};

    for (const auto& pattern : m_patterns)
        projections.push_back(create_projection(pattern, *m_task));

    return projections;
}
}
