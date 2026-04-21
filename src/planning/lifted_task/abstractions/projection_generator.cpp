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

#include "projection_generator/task_projection.hpp"
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

template<f::FactKind T>
bool is_ground(const fp::MutableAtom<T>& atom)
{
    return std::all_of(atom.terms.begin(), atom.terms.end(), [](const auto& term) { return u::is_object(term); });
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

u::SubstitutionFunction<Index<f::Object>> lift_substitution_to_original(const u::SubstitutionFunction<Index<f::Object>>& projected_sigma,
                                                                        size_t original_arity,
                                                                        const std::vector<f::ParameterIndex>& projected_to_original)
{
    auto result = u::SubstitutionFunction<Index<f::Object>>::from_range(f::ParameterIndex { 0 }, original_arity);

    for (uint_t projected_i = 0; projected_i < projected_to_original.size(); ++projected_i)
    {
        const auto projected_p = f::ParameterIndex { projected_i };

        if (!projected_sigma.contains_parameter(projected_p) || !projected_sigma.is_bound(projected_p))
            continue;

        const auto original_p = projected_to_original[projected_i];
        [[maybe_unused]] const auto inserted = result.assign(original_p, *projected_sigma[projected_p]);
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

auto create_abstract_state_changing_transitions(const std::vector<StateView<LiftedTag>>& astates,
                                                const Pattern& pattern,
                                                const ProjectionMapping<LiftedTag>::ActionMapping& projected_to_original_action,
                                                Task<LiftedTag>& task)
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

            for (const auto& [projected_action, info] : projected_to_original_action)
            {
                for_each_unifier(projected_action,
                                 astate_i,
                                 astate_j,
                                 task,
                                 pattern,
                                 [&](const u::SubstitutionFunction<Index<f::Object>>& sigma_projected)
                                 {
                                     const auto sigma_original =
                                         lift_substitution_to_original(sigma_projected, info.original_action.get_arity(), info.projected_to_original);

                                     const auto t = uint_t(transitions.size());
                                     const auto src = uint_t(astate_i.get_index());
                                     const auto dst = uint_t(astate_j.get_index());

                                     transitions.push_back(Transition { projected_action, info.original_action, sigma_original, src, dst });
                                     adj_lists[src].push_back(t);
                                 });
            }
        }
    }

    return std::make_pair(std::move(transitions), std::move(adj_lists));
}

auto create_projection(const Pattern& pattern, const Task<LiftedTag>& original_task)
{
    auto [projected_task, projected_to_original_action] = project_task(original_task, pattern);

    auto state_repository = StateRepository<LiftedTag>::create(projected_task, ExecutionContext::create(1));

    auto [astates, goal_vertices] = create_abstract_states(pattern, *projected_task, *state_repository);
    auto [transitions, adj_lists] = create_abstract_state_changing_transitions(astates, pattern, projected_to_original_action, *projected_task);

    auto result = ProjectionAbstraction(std::make_shared<const ForwardProjectionAbstraction<LiftedTag>>(ProjectionMapping<LiftedTag>(pattern),
                                                                                                        std::move(state_repository),
                                                                                                        std::move(astates),
                                                                                                        std::move(transitions),
                                                                                                        std::move(adj_lists),
                                                                                                        std::move(goal_vertices)));

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