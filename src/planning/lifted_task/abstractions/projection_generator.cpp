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
#include "tyr/planning/lifted_task/state_view.hpp"
#include "tyr/planning/lifted_task/successor_generator.hpp"
#include "tyr/planning/lifted_task/unpacked_state.hpp"

#include <chrono>
#include <limits>

namespace f = tyr::formalism;
namespace fp = tyr::formalism::planning;
namespace u = tyr::formalism::unification;

namespace tyr::planning
{
namespace
{
thread_local auto g_projection_generator_instrumentation = ProjectionGeneratorInstrumentation {};

auto now()
{
    return std::chrono::steady_clock::now();
}

size_t elapsed_ns(std::chrono::steady_clock::time_point start)
{
    return static_cast<size_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now() - start).count());
}
}

void reset_projection_generator_instrumentation()
{
    g_projection_generator_instrumentation = ProjectionGeneratorInstrumentation {};
}

ProjectionGeneratorInstrumentation get_projection_generator_instrumentation()
{
    return g_projection_generator_instrumentation;
}

void add_projection_generator_project_task_breakdown(size_t fdr_context_ns, size_t formalism_task_ns, size_t lifted_task_create_ns)
{
    g_projection_generator_instrumentation.project_fdr_context_ns += fdr_context_ns;
    g_projection_generator_instrumentation.project_formalism_task_ns += formalism_task_ns;
    g_projection_generator_instrumentation.project_lifted_task_create_ns += lifted_task_create_ns;
}

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

struct StaticAtomArgumentKey
{
    fp::PredicateView<f::StaticTag> predicate;
    size_t term_pos;
    Index<f::Object> object;

    auto identifying_members() const noexcept { return std::tie(predicate, term_pos, object); }
};

struct ProjectionContext
{
    std::vector<fp::MutableAtom<f::StaticTag>> static_atoms;
    UnorderedSet<fp::MutableAtom<f::StaticTag>> static_atoms_set;
    UnorderedMap<fp::PredicateView<f::StaticTag>, std::vector<size_t>> static_atoms_by_predicate;
    UnorderedMap<StaticAtomArgumentKey, std::vector<size_t>> static_atoms_by_bound_argument;
    std::vector<size_t> empty_static_atom_indices;
    std::vector<fp::MutableAtom<f::FluentTag>> visible_pattern_atoms;
};

struct AbstractStateInfo
{
    StateView<LiftedTag> state;
    std::vector<fp::MutableAtom<f::FluentTag>> atoms;
};

struct EffectLiteralRef
{
    size_t effect_pos;
    size_t literal_pos;
};

enum class ConditionLiteralKind
{
    Static,
    Fluent,
};

struct ConditionLiteralRef
{
    ConditionLiteralKind kind;
    size_t literal_pos;
};

struct ProjectedActionContext
{
    fp::ActionView projected_action;
    ProjectionMapping<LiftedTag>::ProjectedActionInfo original_info;
    fp::MutableAction mutable_action;
    u::SubstitutionFunction<Data<f::Term>> initial_sigma;
    std::vector<EffectLiteralRef> positive_effect_literals;
    std::vector<EffectLiteralRef> negative_effect_literals;
    std::vector<UnorderedSet<fp::PredicateView<f::FluentTag>>> variable_required_predicates;

    ProjectedActionContext(fp::ActionView projected_action, ProjectionMapping<LiftedTag>::ProjectedActionInfo original_info) :
        projected_action(projected_action),
        original_info(std::move(original_info)),
        mutable_action(projected_action),
        initial_sigma(make_sigma(mutable_action)),
        variable_required_predicates(initial_sigma.size())
    {
        register_required_predicates(mutable_action.condition);

        for (size_t effect_pos = 0; effect_pos < mutable_action.effects.size(); ++effect_pos)
        {
            register_required_predicates(mutable_action.effects[effect_pos].condition);

            const auto& effect_literals = mutable_action.effects[effect_pos].effect.literals;

            for (size_t literal_pos = 0; literal_pos < effect_literals.size(); ++literal_pos)
            {
                register_required_predicate(effect_literals[literal_pos].atom);

                const auto ref = EffectLiteralRef { effect_pos, literal_pos };

                if (effect_literals[literal_pos].polarity)
                    positive_effect_literals.push_back(ref);
                else
                    negative_effect_literals.push_back(ref);
            }
        }
    }

private:
    void register_required_predicate(const fp::MutableAtom<f::FluentTag>& atom)
    {
        for (const auto& term : atom.terms)
        {
            if (u::is_object(term))
                continue;

            const auto parameter = u::get_parameter(term);
            const auto parameter_index = size_t(uint_t(parameter));
            if (parameter_index >= variable_required_predicates.size())
                continue;

            variable_required_predicates[parameter_index].insert(atom.predicate);
        }
    }

    void register_required_predicates(const fp::MutableConjunctiveCondition& condition)
    {
        for (const auto& lit : condition.fluent_literals)
        {
            if (lit.polarity)
                register_required_predicate(lit.atom);
        }
    }
};

template<f::FactKind T>
bool literal_holds(const fp::MutableLiteral<T>& lit, const std::vector<fp::MutableAtom<T>>& atoms)
{
    assert(is_ground(lit.atom));

    if (lit.polarity)
        return contains_atom(atoms, lit.atom);

    return !contains_atom(atoms, lit.atom);
}

bool literal_holds(const fp::MutableLiteral<f::StaticTag>& lit, const UnorderedSet<fp::MutableAtom<f::StaticTag>>& atoms)
{
    assert(is_ground(lit.atom));

    const auto contains = atoms.contains(lit.atom);
    return lit.polarity ? contains : !contains;
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

const std::vector<size_t>& select_static_candidate_indices(const fp::MutableAtom<f::StaticTag>& atom, const ProjectionContext& projection_context);

/**
 * Static literal satisfaction
 *
 * Enumerates all substitutions extending `sigma` that satisfy the static literals.
 */
size_t static_literal_candidate_count(const fp::MutableLiteral<f::StaticTag>& lit,
                                      const ProjectionContext& projection_context,
                                      const u::SubstitutionFunction<Data<f::Term>>& sigma)
{
    const auto grounded_lit = u::apply_substitution_fixpoint(lit, sigma);

    if (is_ground(grounded_lit.atom))
        return literal_holds(grounded_lit, projection_context.static_atoms_set) ? 1 : 0;

    // Negative nonground static literals are not handled existentially here.
    // Defer them in the join order so earlier positive literals can bind them.
    if (!grounded_lit.polarity)
        return std::numeric_limits<size_t>::max();

    return select_static_candidate_indices(grounded_lit.atom, projection_context).size();
}

size_t fluent_literal_candidate_count(const fp::MutableLiteral<f::FluentTag>& lit,
                                      const std::vector<fp::MutableAtom<f::FluentTag>>& src_atoms,
                                      const std::vector<fp::MutableAtom<f::FluentTag>>& visible_pattern_atoms,
                                      const u::SubstitutionFunction<Data<f::Term>>& sigma)
{
    const auto grounded_lit = u::apply_substitution_fixpoint(lit, sigma);

    if (is_ground(grounded_lit.atom))
    {
        if (!is_visible_atom(grounded_lit.atom, visible_pattern_atoms))
            return 1;

        return literal_holds(grounded_lit, src_atoms) ? 1 : 0;
    }

    if (!grounded_lit.polarity)
        return std::numeric_limits<size_t>::max();

    auto result = size_t { 0 };

    for (const auto& atom : src_atoms)
    {
        if (atom.predicate.get_index() != grounded_lit.atom.predicate.get_index())
            continue;

        auto sigma2 = sigma;
        if (match_literal_to_atom(grounded_lit, atom, std::move(sigma2)))
            ++result;
    }

    return result;
}

const std::vector<size_t>& select_static_candidate_indices(const fp::MutableAtom<f::StaticTag>& atom, const ProjectionContext& projection_context)
{
    const auto predicate_it = projection_context.static_atoms_by_predicate.find(atom.predicate);
    const auto* best = predicate_it == projection_context.static_atoms_by_predicate.end() ? &projection_context.empty_static_atom_indices : &predicate_it->second;

    if (best->empty())
        return *best;

    for (size_t term_pos = 0; term_pos < atom.terms.size(); ++term_pos)
    {
        const auto& term = atom.terms[term_pos];
        if (!u::is_object(term))
            continue;

        const auto key = StaticAtomArgumentKey { atom.predicate, term_pos, u::get_object(term) };
        const auto argument_it = projection_context.static_atoms_by_bound_argument.find(key);
        if (argument_it == projection_context.static_atoms_by_bound_argument.end())
            return projection_context.empty_static_atom_indices;

        if (argument_it->second.size() < best->size())
            best = &argument_it->second;
    }

    return *best;
}

size_t static_literal_required_predicate_count(const fp::MutableLiteral<f::StaticTag>& lit,
                                              const ProjectedActionContext& action_context,
                                              const u::SubstitutionFunction<Data<f::Term>>& sigma)
{
    const auto grounded_lit = u::apply_substitution_fixpoint(lit, sigma);
    auto result = size_t { 0 };
    auto seen = UnorderedSet<f::ParameterIndex> {};

    for (const auto& term : grounded_lit.atom.terms)
    {
        if (u::is_object(term))
            continue;

        const auto parameter = u::get_parameter(term);
        if (!seen.insert(parameter).second)
            continue;

        const auto parameter_index = size_t(uint_t(parameter));
        if (parameter_index >= action_context.variable_required_predicates.size())
            continue;

        result += action_context.variable_required_predicates[parameter_index].size();
    }

    return result;
}

size_t condition_literal_required_predicate_count(const fp::MutableConjunctiveCondition& condition,
                                                 const ConditionLiteralRef& ref,
                                                 const ProjectedActionContext& action_context,
                                                 const u::SubstitutionFunction<Data<f::Term>>& sigma)
{
    if (ref.kind == ConditionLiteralKind::Static)
        return static_literal_required_predicate_count(condition.static_literals[ref.literal_pos], action_context, sigma);

    return size_t { 0 };
}

size_t condition_literal_candidate_count(const fp::MutableConjunctiveCondition& condition,
                                         const ConditionLiteralRef& ref,
                                         const std::vector<fp::MutableAtom<f::FluentTag>>& src_atoms,
                                         const ProjectionContext& projection_context,
                                         const u::SubstitutionFunction<Data<f::Term>>& sigma)
{
    if (ref.kind == ConditionLiteralKind::Static)
        return static_literal_candidate_count(condition.static_literals[ref.literal_pos], projection_context, sigma);

    return fluent_literal_candidate_count(condition.fluent_literals[ref.literal_pos], src_atoms, projection_context.visible_pattern_atoms, sigma);
}

size_t select_next_condition_literal(const fp::MutableConjunctiveCondition& condition,
                                     std::vector<ConditionLiteralRef>& literal_order,
                                     size_t pos,
                                     const std::vector<fp::MutableAtom<f::FluentTag>>& src_atoms,
                                     const ProjectionContext& projection_context,
                                     const ProjectedActionContext& action_context,
                                     const u::SubstitutionFunction<Data<f::Term>>& sigma)
{
    auto best_pos = pos;
    auto best_count = std::numeric_limits<size_t>::max();
    auto best_required_predicate_count = size_t { 0 };
    auto best_is_fluent = false;

    for (size_t i = pos; i < literal_order.size(); ++i)
    {
        const auto& ref = literal_order[i];
        const auto count = condition_literal_candidate_count(condition, ref, src_atoms, projection_context, sigma);
        const auto required_predicate_count = condition_literal_required_predicate_count(condition, ref, action_context, sigma);
        const auto is_fluent = ref.kind == ConditionLiteralKind::Fluent;

        if (count < best_count || (count == best_count && is_fluent && !best_is_fluent)
            || (count == best_count && is_fluent == best_is_fluent && required_predicate_count > best_required_predicate_count))
        {
            best_pos = i;
            best_count = count;
            best_required_predicate_count = required_predicate_count;
            best_is_fluent = is_fluent;

            if (best_count == 0)
                break;
        }
    }

    return best_pos;
}

template<typename Callback>
void satisfy_condition_literals_rec(const fp::MutableConjunctiveCondition& condition,
                                    std::vector<ConditionLiteralRef>& literal_order,
                                    size_t pos,
                                    const std::vector<fp::MutableAtom<f::FluentTag>>& src_atoms,
                                    const ProjectionContext& projection_context,
                                    const ProjectedActionContext& action_context,
                                    const u::SubstitutionFunction<Data<f::Term>>& sigma,
                                    Callback&& callback)
{
    if (pos == literal_order.size())
    {
        callback(sigma);
        return;
    }

    const auto selected_pos = select_next_condition_literal(condition, literal_order, pos, src_atoms, projection_context, action_context, sigma);
    std::swap(literal_order[pos], literal_order[selected_pos]);
    const auto ref = literal_order[pos];
    const auto selected_candidate_count = condition_literal_candidate_count(condition, ref, src_atoms, projection_context, sigma);

    if (ref.kind == ConditionLiteralKind::Static)
        ++g_projection_generator_instrumentation.selected_static_literals;
    else
        ++g_projection_generator_instrumentation.selected_fluent_literals;

    if (selected_candidate_count == 0)
        ++g_projection_generator_instrumentation.selected_zero_candidate_literals;

    if (ref.kind == ConditionLiteralKind::Fluent)
    {
        const auto lit = u::apply_substitution_fixpoint(condition.fluent_literals[ref.literal_pos], sigma);

        if (is_ground(lit.atom))
        {
            if (!is_visible_atom(lit.atom, projection_context.visible_pattern_atoms) || literal_holds(lit, src_atoms))
                satisfy_condition_literals_rec(condition,
                                               literal_order,
                                               pos + 1,
                                               src_atoms,
                                               projection_context,
                                               action_context,
                                               sigma,
                                               std::forward<Callback>(callback));

            std::swap(literal_order[pos], literal_order[selected_pos]);
            return;
        }

        if (!lit.polarity)
        {
            satisfy_condition_literals_rec(condition,
                                           literal_order,
                                           pos + 1,
                                           src_atoms,
                                           projection_context,
                                           action_context,
                                           sigma,
                                           std::forward<Callback>(callback));

            std::swap(literal_order[pos], literal_order[selected_pos]);
            return;
        }

        for (const auto& atom : src_atoms)
        {
            if (atom.predicate.get_index() != lit.atom.predicate.get_index())
                continue;

            ++g_projection_generator_instrumentation.fluent_candidate_atoms_tried;

            auto sigma2 = sigma;
            const auto matched = match_literal_to_atom(lit, atom, std::move(sigma2));
            if (!matched)
                continue;

            ++g_projection_generator_instrumentation.fluent_visible_branches;

            satisfy_condition_literals_rec(condition,
                                           literal_order,
                                           pos + 1,
                                           src_atoms,
                                           projection_context,
                                           action_context,
                                           *matched,
                                           std::forward<Callback>(callback));
        }

        // Hidden fluent support is existential. Keep the unbound branch after
        // visible matches, so the small visible fluent joins bind variables early.
        ++g_projection_generator_instrumentation.fluent_hidden_branches;

        satisfy_condition_literals_rec(condition,
                                       literal_order,
                                       pos + 1,
                                       src_atoms,
                                       projection_context,
                                       action_context,
                                       sigma,
                                       std::forward<Callback>(callback));

        std::swap(literal_order[pos], literal_order[selected_pos]);
        return;
    }

    const auto lit = u::apply_substitution_fixpoint(condition.static_literals[ref.literal_pos], sigma);

    if (is_ground(lit.atom))
    {
        if (literal_holds(lit, projection_context.static_atoms_set))
            satisfy_condition_literals_rec(condition,
                                           literal_order,
                                           pos + 1,
                                           src_atoms,
                                           projection_context,
                                           action_context,
                                           sigma,
                                           std::forward<Callback>(callback));

        std::swap(literal_order[pos], literal_order[selected_pos]);

        return;
    }

    // Negative nonground static literals are not handled existentially here.
    if (!lit.polarity)
    {
        std::swap(literal_order[pos], literal_order[selected_pos]);
        return;
    }

    const auto& candidate_indices = select_static_candidate_indices(lit.atom, projection_context);

    for (const auto atom_index : candidate_indices)
    {
        ++g_projection_generator_instrumentation.static_candidate_atoms_tried;

        const auto& atom = projection_context.static_atoms[atom_index];
        auto sigma2 = sigma;
        const auto matched = match_literal_to_atom(lit, atom, std::move(sigma2));
        if (!matched)
            continue;

        satisfy_condition_literals_rec(condition,
                                       literal_order,
                                       pos + 1,
                                       src_atoms,
                                       projection_context,
                                       action_context,
                                       *matched,
                                       std::forward<Callback>(callback));
    }

    std::swap(literal_order[pos], literal_order[selected_pos]);
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
                                const std::vector<fp::MutableAtom<f::FluentTag>>& src_atoms,
                                const ProjectionContext& projection_context,
                                const ProjectedActionContext& action_context,
                                const u::SubstitutionFunction<Data<f::Term>>& sigma,
                                Callback&& callback)
{
    ++g_projection_generator_instrumentation.condition_join_calls;

    auto finish = [&](const u::SubstitutionFunction<Data<f::Term>>& sigma2)
    {
        if (!visible_fluent_literals_hold_in_src(condition.fluent_literals, src_atoms, projection_context.visible_pattern_atoms, sigma2))
            return;

        ++g_projection_generator_instrumentation.condition_binding_callbacks;
        callback(sigma2);
    };

    auto literal_order = std::vector<ConditionLiteralRef> {};
    literal_order.reserve(condition.static_literals.size() + condition.fluent_literals.size());
    for (size_t i = 0; i < condition.static_literals.size(); ++i)
        literal_order.push_back(ConditionLiteralRef { ConditionLiteralKind::Static, i });
    for (size_t i = 0; i < condition.fluent_literals.size(); ++i)
        literal_order.push_back(ConditionLiteralRef { ConditionLiteralKind::Fluent, i });

    satisfy_condition_literals_rec(condition,
                                   literal_order,
                                   0,
                                   src_atoms,
                                   projection_context,
                                   action_context,
                                   sigma,
                                   [&](const u::SubstitutionFunction<Data<f::Term>>& sigma2)
                                   {
                                       finish(sigma2);
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
                       const std::vector<EffectLiteralRef>& positive_effect_literals,
                       const std::vector<EffectLiteralRef>& negative_effect_literals,
                       const std::vector<fp::MutableAtom<f::FluentTag>>& added,
                       const std::vector<fp::MutableAtom<f::FluentTag>>& deleted,
                       size_t add_pos,
                       size_t del_pos,
                       const u::SubstitutionFunction<Data<f::Term>>& sigma,
                       Callback&& callback)
{
    ++g_projection_generator_instrumentation.change_unification_calls;

    if (add_pos == added.size() && del_pos == deleted.size())
    {
        callback(sigma);
        return;
    }

    if (add_pos < added.size())
    {
        const auto& target = added[add_pos];

        for (const auto ref : positive_effect_literals)
        {
            ++g_projection_generator_instrumentation.positive_effect_candidates_tried;

            const auto& lit = action.effects[ref.effect_pos].effect.literals[ref.literal_pos];

            auto sigma2 = sigma;
            const auto matched = match_effect_literal(lit, target, true, std::move(sigma2));
            if (!matched)
                continue;

            unify_changes_rec(action,
                              positive_effect_literals,
                              negative_effect_literals,
                              added,
                              deleted,
                              add_pos + 1,
                              del_pos,
                              *matched,
                              std::forward<Callback>(callback));
        }

        return;
    }

    const auto& target = deleted[del_pos];

    for (const auto ref : negative_effect_literals)
    {
        ++g_projection_generator_instrumentation.negative_effect_candidates_tried;

        const auto& lit = action.effects[ref.effect_pos].effect.literals[ref.literal_pos];

        auto sigma2 = sigma;
        const auto matched = match_effect_literal(lit, target, false, std::move(sigma2));
        if (!matched)
            continue;

        unify_changes_rec(action,
                          positive_effect_literals,
                          negative_effect_literals,
                          added,
                          deleted,
                          add_pos,
                          del_pos + 1,
                          *matched,
                          std::forward<Callback>(callback));
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
                                     const ProjectionContext& projection_context,
                                     const ProjectedActionContext& action_context,
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

        ++g_projection_generator_instrumentation.verified_binding_callbacks;
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
                                    projection_context,
                                    action_context,
                                    sigma,
                                    produced_add,
                                    produced_del,
                                    std::forward<Callback>(callback));

    // Option 2: satisfy the effect condition and realize its visible consequences.
    ++g_projection_generator_instrumentation.effect_condition_fire_attempts;

    satisfy_condition_bindings(ceff.condition,
                               src_atoms,
                               projection_context,
                               action_context,
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

                                       if (!is_visible_atom(lit.atom, projection_context.visible_pattern_atoms))
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
                                                                   projection_context,
                                                                   action_context,
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
void for_each_unifier(const ProjectedActionContext& action_context,
                      const AbstractStateInfo& src,
                      const AbstractStateInfo& dst,
                      const ProjectionContext& projection_context,
                      Callback&& callback)
{
    ++g_projection_generator_instrumentation.unifier_queries;

    const auto& mutable_action = action_context.mutable_action;
    const auto& src_atoms = src.atoms;
    const auto& dst_atoms = dst.atoms;

    // Pattern-visible difference only.
    const auto added = difference_atoms(dst_atoms, src_atoms);
    const auto deleted = difference_atoms(src_atoms, dst_atoms);

    auto seen = UnorderedSet<u::SubstitutionFunction<Index<f::Object>>> {};

    unify_changes_rec(mutable_action,
                      action_context.positive_effect_literals,
                      action_context.negative_effect_literals,
                      added,
                      deleted,
                      0,
                      0,
                      action_context.initial_sigma,
                      [&](const u::SubstitutionFunction<Data<f::Term>>& sigma0)
                      {
                          satisfy_condition_bindings(mutable_action.condition,
                                                     src_atoms,
                                                     projection_context,
                                                     action_context,
                                                     sigma0,
                                                     [&](const u::SubstitutionFunction<Data<f::Term>>& sigma1)
                                                     {
                                                         enumerate_verified_bindings_rec(mutable_action,
                                                                                         0,
                                                                                         src_atoms,
                                                                                         dst_atoms,
                                                                                         added,
                                                                                         deleted,
                                                                                         projection_context,
                                                                                         action_context,
                                                                                         sigma1,
                                                                                         {},
                                                                                         {},
                                                                                         [&](const u::SubstitutionFunction<Data<f::Term>>& sigma_final)
                                                                                         {
                                                                                             const auto obj_sigma =
                                                                                                 to_object_substitution(sigma_final,
                                                                                                                        action_context.projected_action.get_arity());
                                                                                             if (!obj_sigma)
                                                                                                 return;

                                                                                             if (!seen.insert(*obj_sigma).second)
                                                                                                 return;

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

auto create_abstract_state_infos(const std::vector<StateView<LiftedTag>>& astates, const Pattern& pattern)
{
    auto result = std::vector<AbstractStateInfo> {};
    result.reserve(astates.size());

    for (const auto& astate : astates)
        result.push_back(AbstractStateInfo { astate, collect_visible_fluent_atoms(astate, pattern) });

    return result;
}

auto create_projected_action_contexts(const ProjectionMapping<LiftedTag>::ActionMapping& projected_to_original_action)
{
    auto result = std::vector<ProjectedActionContext> {};
    result.reserve(projected_to_original_action.size());

    for (const auto& [projected_action, info] : projected_to_original_action)
        result.emplace_back(projected_action, info);

    return result;
}

auto create_projection_context(const Pattern& pattern, const Task<LiftedTag>& task)
{
    auto static_atoms = collect_projected_static_atoms(task);
    auto static_atoms_set = UnorderedSet<fp::MutableAtom<f::StaticTag>> {};
    auto static_atoms_by_predicate = UnorderedMap<fp::PredicateView<f::StaticTag>, std::vector<size_t>> {};
    auto static_atoms_by_bound_argument = UnorderedMap<StaticAtomArgumentKey, std::vector<size_t>> {};
    static_atoms_set.reserve(static_atoms.size());

    for (size_t i = 0; i < static_atoms.size(); ++i)
    {
        const auto& atom = static_atoms[i];
        static_atoms_set.insert(atom);
        static_atoms_by_predicate[atom.predicate].push_back(i);

        for (size_t term_pos = 0; term_pos < atom.terms.size(); ++term_pos)
        {
            const auto& term = atom.terms[term_pos];
            assert(u::is_object(term));

            const auto key = StaticAtomArgumentKey { atom.predicate, term_pos, u::get_object(term) };
            static_atoms_by_bound_argument[key].push_back(i);
        }
    }

    return ProjectionContext { std::move(static_atoms),
                               std::move(static_atoms_set),
                               std::move(static_atoms_by_predicate),
                               std::move(static_atoms_by_bound_argument),
                               {},
                               collect_pattern_atoms(pattern) };
}

auto create_abstract_state_changing_transitions(const std::vector<StateView<LiftedTag>>& astates,
                                                const Pattern& pattern,
                                                const ProjectionMapping<LiftedTag>::ActionMapping& projected_to_original_action,
                                                Task<LiftedTag>& task)
{
    const auto transition_generation_start = now();

    const auto projection_context_start = now();
    const auto projection_context = create_projection_context(pattern, task);
    g_projection_generator_instrumentation.projection_context_ns += elapsed_ns(projection_context_start);

    const auto abstract_state_infos_start = now();
    const auto astate_infos = create_abstract_state_infos(astates, pattern);
    g_projection_generator_instrumentation.abstract_state_infos_ns += elapsed_ns(abstract_state_infos_start);

    const auto action_contexts_start = now();
    const auto action_contexts = create_projected_action_contexts(projected_to_original_action);
    g_projection_generator_instrumentation.action_contexts_created += action_contexts.size();
    g_projection_generator_instrumentation.action_contexts_ns += elapsed_ns(action_contexts_start);

    auto transitions = TransitionList {};
    auto adj_lists = std::vector<std::vector<uint_t>>(astates.size());

    const auto transition_loop_start = now();

    for (size_t i = 0; i < astate_infos.size(); ++i)
    {
        const auto& astate_i = astate_infos[i];

        for (size_t j = 0; j < astate_infos.size(); ++j)
        {
            ++g_projection_generator_instrumentation.state_pair_checks;

            if (i == j)
            {
                ++g_projection_generator_instrumentation.self_state_pairs_skipped;
                continue;
            }

            const auto& astate_j = astate_infos[j];

            for (const auto& action_context : action_contexts)
            {
                ++g_projection_generator_instrumentation.action_pair_checks;

                for_each_unifier(action_context,
                                 astate_i,
                                 astate_j,
                                 projection_context,
                                 [&](const u::SubstitutionFunction<Index<f::Object>>& sigma_projected)
                                 {
                                     const auto sigma_original =
                                         lift_substitution_to_original(sigma_projected,
                                                                      action_context.original_info.original_action.get_arity(),
                                                                      action_context.original_info.projected_to_original);

                                     const auto t = uint_t(transitions.size());
                                     const auto src = uint_t(astate_i.state.get_index());
                                     const auto dst = uint_t(astate_j.state.get_index());

                                     transitions.push_back(
                                         Transition { action_context.projected_action, action_context.original_info.original_action, sigma_original, src, dst });
                                     adj_lists[src].push_back(t);
                                 });
            }
        }
    }

    g_projection_generator_instrumentation.transition_loop_ns += elapsed_ns(transition_loop_start);
    g_projection_generator_instrumentation.transition_generation_ns += elapsed_ns(transition_generation_start);

    return std::make_pair(std::move(transitions), std::move(adj_lists));
}

auto create_projection(const Pattern& pattern, const Task<LiftedTag>& original_task)
{
    const auto projection_start = now();
    ++g_projection_generator_instrumentation.patterns_processed;

    const auto project_task_start = now();
    auto [projected_task, projected_to_original_action] = project_task(original_task, pattern);
    g_projection_generator_instrumentation.project_task_ns += elapsed_ns(project_task_start);

    auto state_repository = StateRepository<LiftedTag>::create(projected_task, ExecutionContext::create(1));

    const auto abstract_states_start = now();
    auto [astates, goal_vertices] = create_abstract_states(pattern, *projected_task, *state_repository);
    g_projection_generator_instrumentation.abstract_states_created += astates.size();
    g_projection_generator_instrumentation.abstract_states_ns += elapsed_ns(abstract_states_start);

    auto [transitions, adj_lists] = create_abstract_state_changing_transitions(astates, pattern, projected_to_original_action, *projected_task);

    const auto projection_assembly_start = now();
    auto result = ProjectionAbstraction(std::make_shared<const ForwardProjectionAbstraction<LiftedTag>>(ProjectionMapping<LiftedTag>(pattern),
                                                                                                        std::move(state_repository),
                                                                                                        std::move(astates),
                                                                                                        std::move(transitions),
                                                                                                        std::move(adj_lists),
                                                                                                        std::move(goal_vertices)));
    g_projection_generator_instrumentation.projection_assembly_ns += elapsed_ns(projection_assembly_start);
    g_projection_generator_instrumentation.total_projection_ns += elapsed_ns(projection_start);

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
