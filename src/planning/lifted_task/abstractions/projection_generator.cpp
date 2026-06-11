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

#include "projection_generator/projection_join_plan.hpp"
#include "projection_generator/task_projection.hpp"
#include "tyr/planning/lifted_task/abstractions/relaxed_reachability.hpp"
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
#include "tyr/planning/lifted_task/node.hpp"
#include "tyr/planning/lifted_task/state_view.hpp"
#include "tyr/planning/lifted_task/successor_generator.hpp"
#include "tyr/planning/lifted_task/unpacked_state.hpp"

#include <iostream>
#include <optional>

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

// True iff every term of `atom` resolves to an object under `sigma`, i.e.
// `is_ground(apply_substitution_fixpoint(atom, sigma))` — but computed per term
// without materializing the substituted atom (the scalar fixpoint on a term does
// not allocate). Lets the enumeration decide ground-vs-non-ground cheaply and
// skip building a MutableLiteral on the non-ground (match) path.
template<f::FactKind T>
bool atom_ground_under(const fp::MutableAtom<T>& atom, const u::SubstitutionFunction<Data<f::Term>>& sigma)
{
    for (const auto& term : atom.terms)
    {
        if (!u::is_object(u::apply_substitution_fixpoint(term, sigma)))
            return false;
    }
    return true;
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

// Build a compact, hashable dedup key from an object substitution. All object
// substitutions produced for a given projected action share the same parameter
// domain (from_range(0, arity)) and parameter ordering, so the bound-object
// indices alone (positionally) uniquely identify the substitution — equivalent
// to EqualTo over the full SubstitutionFunction but cheap to hash. 0 = unbound,
// object index o is stored as o+1.
inline std::vector<std::uint32_t> object_substitution_key(const u::SubstitutionFunction<Index<f::Object>>& s)
{
    const auto& data = std::get<1>(s.identifying_members());
    auto key = std::vector<std::uint32_t>(data.size(), 0u);
    for (std::size_t i = 0; i < data.size(); ++i)
        if (data[i].has_value())
            key[i] = static_cast<std::uint32_t>(uint_t(*data[i])) + 1u;
    return key;
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

// ---------------------------------------------------------------------------
// Phase 2: O(2^k) forward transition enumeration helpers
// ---------------------------------------------------------------------------

// Maps each pattern atom to its bit index (== position in Pattern::facts).
UnorderedMap<fp::MutableAtom<f::FluentTag>, uint_t> build_pattern_bit_index(const Pattern& pattern)
{
    auto result = UnorderedMap<fp::MutableAtom<f::FluentTag>, uint_t> {};
    result.reserve(pattern.facts.size());
    for (uint_t i = 0; i < uint_t(pattern.facts.size()); ++i)
        result.emplace(fp::MutableAtom<f::FluentTag>(pattern.facts[i].get_atom().value()), i);
    return result;
}

// Predicate → set of reachable ground-atom object tuples. Populated from
// `RelaxedReachability<LiftedTag>::compute()` when the `reachability_filter`
// projection option is on. Used by `verify_pattern_preconditions` to drop
// transitions whose ground non-pattern positive preconditions are absent
// from R+ — emulating Scorpion's operator-level reachability pruning.
using ReachableAtomIndex = UnorderedMap<fp::PredicateView<f::FluentTag>, UnorderedSet<std::vector<std::uint32_t>>>;

// Build a hashable tuple from a ground atom's term list.
inline std::vector<std::uint32_t> atom_to_obj_tuple(const fp::MutableAtom<f::FluentTag>& atom)
{
    auto tup = std::vector<std::uint32_t> {};
    tup.reserve(atom.terms.size());
    for (const auto& term : atom.terms)
        tup.push_back(static_cast<std::uint32_t>(uint_t(u::get_object(term))));
    return tup;
}

// Predicate-only lookup with empty fallback.
inline bool atom_in_reachable_index(const fp::MutableAtom<f::FluentTag>& atom, const ReachableAtomIndex* index)
{
    if (index == nullptr)
        return true;  // R+ filter disabled → over-approximate
    const auto it = index->find(atom.predicate);
    if (it == index->end())
        return false;
    return it->second.contains(atom_to_obj_tuple(atom));
}

// After the FULL substitution (precondition + effect-side bindings) is
// determined, verify positive precondition literals.
//
// Two cases:
//  (a) The grounded precondition is a pattern atom → must be set in src_mask;
//      otherwise drop the transition.
//  (b) The grounded precondition is NOT a pattern atom (or still non-ground)
//      → *normally* over-approximate as satisfied. **Tightening (non-ground
//      case)**: enumerate compatible pattern atoms (predicate match, bound
//      terms agree). If at least one is true in src, satisfied. Otherwise,
//      compute the typed-domain product over still-unbound positions; if it
//      equals the count of compatible pattern atoms, every possible ground
//      candidate is a pattern atom — since none are in src, no satisfying
//      ground atom exists, drop. Otherwise some candidate would be a
//      non-pattern atom (which the abstraction doesn't track), keep.
//
// `param_domain_sizes[i]` = size of the projected action's parameter `i`'s
// typed domain. 0 is a sentinel "unknown" (e.g. ceff-local params); when
// encountered, the tightening conservatively skips that literal.
bool verify_pattern_preconditions(const std::vector<fp::MutableLiteral<f::FluentTag>>& positive_fluent,
                                  const u::SubstitutionFunction<Data<f::Term>>& sigma,
                                  const std::vector<fp::MutableAtom<f::FluentTag>>& pattern_atoms,
                                  const UnorderedMap<fp::MutableAtom<f::FluentTag>, uint_t>& atom_bit_index,
                                  const std::vector<std::size_t>& param_domain_sizes,
                                  const ReachableAtomIndex* reachable_index,
                                  uint_t src_mask)
{
    for (const auto& lit : positive_fluent)
    {
        const auto grounded = u::apply_substitution_fixpoint(lit, sigma);
        if (is_ground(grounded.atom))
        {
            const auto it = atom_bit_index.find(grounded.atom);
            if (it == atom_bit_index.end())
            {
                // Ground non-pattern atom: standard PDB semantics says
                // over-approximate as satisfied. With R+ filter on, do the
                // operator-level reachability check Scorpion's grounder
                // does — if the atom is not in R+ no real grounding can
                // satisfy this precondition, so drop the transition.
                if (reachable_index != nullptr && !atom_in_reachable_index(grounded.atom, reachable_index))
                    return false;
                continue;
            }
            const uint_t bit = uint_t(1) << it->second;
            if ((src_mask & bit) == 0)
                return false;
            continue;
        }

        // Non-ground tightening.
        bool any_in_src = false;
        std::size_t compatible_count = 0;
        for (const auto& pa : pattern_atoms)
        {
            if (pa.predicate.get_index() != grounded.atom.predicate.get_index())
                continue;
            if (pa.terms.size() != grounded.atom.terms.size())
                continue;
            bool compat = true;
            for (std::size_t i = 0; i < grounded.atom.terms.size(); ++i)
            {
                if (u::is_object(grounded.atom.terms[i]))
                {
                    if (!u::is_object(pa.terms[i]) || u::get_object(pa.terms[i]) != u::get_object(grounded.atom.terms[i]))
                    {
                        compat = false;
                        break;
                    }
                }
            }
            if (!compat)
                continue;
            ++compatible_count;
            const auto it = atom_bit_index.find(pa);
            if (it != atom_bit_index.end())
            {
                const uint_t bit = uint_t(1) << it->second;
                if ((src_mask & bit) != 0)
                {
                    any_in_src = true;
                    break;
                }
            }
        }
        if (any_in_src)
            continue;

        // Typed-domain product over still-unbound positions.
        std::size_t candidate_count = 1;
        bool unknown = false;
        for (std::size_t i = 0; i < grounded.atom.terms.size(); ++i)
        {
            if (u::is_object(grounded.atom.terms[i]))
                continue;
            if (!u::is_parameter(grounded.atom.terms[i]))
            {
                unknown = true;
                break;
            }
            const auto pi = static_cast<std::size_t>(uint_t(u::get_parameter(grounded.atom.terms[i])));
            if (pi >= param_domain_sizes.size() || param_domain_sizes[pi] == 0)
            {
                unknown = true;
                break;
            }
            const auto sz = param_domain_sizes[pi];
            if (candidate_count != 0 && sz > std::numeric_limits<std::size_t>::max() / candidate_count)
            {
                unknown = true;
                break;
            }
            candidate_count *= sz;
        }
        if (unknown)
            continue;
        if (candidate_count > compatible_count)
            continue;

        // All candidates are pattern atoms; none are in src → drop.
        return false;
    }
    return true;
}

// Apply one effect literal (after substitution) to dst_mask.
// Non-ground literals (params bound to non-pattern objects) are no-ops.
void apply_effect_to_mask(const fp::MutableLiteral<f::FluentTag>& lit,
                          const u::SubstitutionFunction<Data<f::Term>>& sigma,
                          const UnorderedMap<fp::MutableAtom<f::FluentTag>, uint_t>& atom_bit_index,
                          uint_t& dst_mask)
{
    const auto grounded = u::apply_substitution_fixpoint(lit, sigma);
    if (!is_ground(grounded.atom))
        return;
    const auto it = atom_bit_index.find(grounded.atom);
    if (it == atom_bit_index.end())
        return;
    const uint_t bit = uint_t(1) << it->second;
    if (grounded.polarity)
        dst_mask |= bit;
    else
        dst_mask &= ~bit;
}

// Phase 4b: per-src-state index over visible fluent atoms, keyed by predicate.
// nullptr means "index disabled" — caller falls back to linear scan over src_atoms.
using SrcAtomsByPredicate = UnorderedMap<fp::PredicateView<f::FluentTag>, std::vector<fp::MutableAtom<f::FluentTag>>>;

SrcAtomsByPredicate build_src_atoms_index(const std::vector<fp::MutableAtom<f::FluentTag>>& src_atoms)
{
    auto index = SrcAtomsByPredicate {};
    for (const auto& atom : src_atoms)
        index[atom.predicate].push_back(atom);
    return index;
}

// Phase 4c helper: returns false if the negative literal is ground and holds in
// src_atoms (i.e. the negation is violated). Non-ground literals pass through
// (existential treatment).
bool check_one_negative(const fp::MutableLiteral<f::FluentTag>& lit,
                        const std::vector<fp::MutableAtom<f::FluentTag>>& src_atoms,
                        const u::SubstitutionFunction<Data<f::Term>>& sigma)
{
    const auto grounded = u::apply_substitution_fixpoint(lit, sigma);
    if (!is_ground(grounded.atom))
        return true;
    return !contains_atom(src_atoms, grounded.atom);
}

// Phase 4d helper: check a `(not (= t1 t2))` constraint by direct object-identity
// comparison. Returns false if both terms ground to the same object (constraint
// violated). Non-ground after substitution → pass via existential rule. Callers
// only schedule binary `=`-literals here (see is_inequality_literal in projection_join_plan.cpp).
bool check_one_inequality(const fp::MutableLiteral<f::StaticTag>& lit,
                          const u::SubstitutionFunction<Data<f::Term>>& sigma)
{
    const auto grounded = u::apply_substitution_fixpoint(lit, sigma);
    const auto& t0 = grounded.atom.terms[0];
    const auto& t1 = grounded.atom.terms[1];
    if (!u::is_object(t0) || !u::is_object(t1))
        return true;  // some term still unbound → existential pass
    return u::get_object(t0) != u::get_object(t1);
}

// Enumerate bindings satisfying the positive fluent precondition literals, with
// negative-literal pushdown (Phase 4c) interleaved: at each recursion level pos,
// fire the negative-literal checks scheduled for that checkpoint before any
// further work, pruning whole subtrees as soon as the negation is violated.
//
// Ground literals: exact check vs src_atoms.
// Non-ground literals: enumerate from src_atoms AND allow existential (non-pattern
// binding) to match the old code's treatment of unbound precondition params as
// existentially held.
//
// When src_atoms_index is non-null (Phase 4b on), non-ground literals iterate
// only the atoms whose predicate matches, instead of scanning all src_atoms.
template<typename Callback>
void enumerate_fluent_pos_rec(const std::vector<fp::MutableLiteral<f::FluentTag>>& positive_fluent,
                              const std::vector<fp::MutableLiteral<f::FluentTag>>& negative_fluent,
                              const std::vector<std::vector<size_t>>& negatives_at_checkpoint,
                              const std::vector<fp::MutableLiteral<f::StaticTag>>& inequalities,
                              const std::vector<std::vector<size_t>>& inequalities_at_checkpoint,
                              size_t pos,
                              const std::vector<fp::MutableAtom<f::FluentTag>>& src_atoms,
                              const SrcAtomsByPredicate* src_atoms_index,
                              const u::SubstitutionFunction<Data<f::Term>>& sigma,
                              Callback&& callback)
{
    // Fire any negative-literal checks scheduled at this checkpoint. With pushdown
    // off, this is a no-op until pos == positive_fluent.size().
    for (const auto neg_idx : negatives_at_checkpoint[pos])
    {
        if (!check_one_negative(negative_fluent[neg_idx], src_atoms, sigma))
            return;
    }

    // Phase 4d: fire any inequality constraints scheduled at this checkpoint.
    // The schedule vector is empty when inequality propagation is off (in which case
    // the literals stay in static_join and are handled there).
    if (!inequalities_at_checkpoint.empty())
    {
        for (const auto ineq_idx : inequalities_at_checkpoint[pos])
        {
            if (!check_one_inequality(inequalities[ineq_idx], sigma))
                return;
        }
    }

    if (pos == positive_fluent.size())
    {
        callback(sigma);
        return;
    }

    const auto& plit = positive_fluent[pos];

    if (atom_ground_under(plit.atom, sigma))
    {
        // Ground under sigma: materialize once and test exact membership in src.
        const auto lit = u::apply_substitution_fixpoint(plit, sigma);
        if (contains_atom(src_atoms, lit.atom))
            enumerate_fluent_pos_rec(positive_fluent, negative_fluent, negatives_at_checkpoint, inequalities, inequalities_at_checkpoint, pos + 1, src_atoms, src_atoms_index, sigma, std::forward<Callback>(callback));
        return;
    }

    // Non-ground: enumerate from visible src atoms. Match the ORIGINAL literal
    // under sigma — match() resolves its parameters through sigma, so this is
    // equivalent to matching the substituted literal but avoids materializing it.
    if (src_atoms_index)
    {
        const auto it = src_atoms_index->find(plit.atom.predicate);
        if (it != src_atoms_index->end())
        {
            for (const auto& atom : it->second)
            {
                auto sigma2 = sigma;
                const auto matched = match_literal_to_atom(plit, atom, std::move(sigma2));
                if (!matched)
                    continue;
                enumerate_fluent_pos_rec(positive_fluent, negative_fluent, negatives_at_checkpoint, inequalities, inequalities_at_checkpoint, pos + 1, src_atoms, src_atoms_index, *matched, callback);
            }
        }
    }
    else
    {
        for (const auto& atom : src_atoms)
        {
            auto sigma2 = sigma;
            const auto matched = match_literal_to_atom(plit, atom, std::move(sigma2));
            if (!matched)
                continue;
            enumerate_fluent_pos_rec(positive_fluent, negative_fluent, negatives_at_checkpoint, inequalities, inequalities_at_checkpoint, pos + 1, src_atoms, src_atoms_index, *matched, callback);
        }
    }

    // Existential branch: leave the positive precondition unsatisfied here
    // and recurse. This over-approximation is needed for lifted PDBs because
    // the precondition's parameters may legitimately bind to non-pattern
    // objects (then the grounded precondition is NOT a pattern atom and the
    // abstract state doesn't track it, so we over-approximate as satisfied).
    // **However**: when effect-side enumeration later binds the params to
    // PATTERN objects — so the grounded precondition IS a pattern atom — the
    // over-approximation is unsafe and produces spurious abstract transitions.
    // The post-check `verify_pattern_preconditions` at transition-emit time
    // (see `create_abstract_state_changing_transitions_v2`) catches that case.
    enumerate_fluent_pos_rec(positive_fluent, negative_fluent, negatives_at_checkpoint, inequalities, inequalities_at_checkpoint, pos + 1, src_atoms, src_atoms_index, sigma, std::forward<Callback>(callback));
}

// Process static join steps using the pre-built StaticAtomIndex.
template<typename Callback>
void join_static_v2(const std::vector<JoinStep>& steps,
                    size_t pos,
                    const StaticAtomIndex& static_index,
                    const u::SubstitutionFunction<Data<f::Term>>& sigma,
                    Callback&& callback)
{
    if (pos == steps.size())
    {
        callback(sigma);
        return;
    }

    const auto& step_lit = steps[pos].literal;

    if (atom_ground_under(step_lit.atom, sigma))
    {
        // Ground under sigma: materialize once and do O(1) hash membership.
        // `literal_holds` semantics: a positive literal holds iff present; a
        // negative literal holds iff absent.
        const auto partial = u::apply_substitution_fixpoint(step_lit, sigma);
        const bool present = static_index.contains(partial.atom);
        if (partial.polarity ? present : !present)
            join_static_v2(steps, pos + 1, static_index, sigma, std::forward<Callback>(callback));
        return;
    }

    if (!step_lit.polarity)
        return;  // negative non-ground static: skip (matches old code)

    // Match the ORIGINAL literal under sigma (predicate is unchanged by
    // substitution; match() resolves its parameters through sigma), avoiding a
    // per-node materialization of the substituted literal.
    for (const auto& atom : static_index.lookup(step_lit.atom.predicate))
    {
        auto sigma2 = sigma;
        const auto matched = match_literal_to_atom(step_lit, atom, std::move(sigma2));
        if (!matched)
            continue;
        join_static_v2(steps, pos + 1, static_index, *matched, callback);
    }
    // No existential case for static literals (they are fully grounded in the task).
}

// Enumerate all sigma extensions satisfying a conjunctive condition.
// Negative fluent literals are checked inside enumerate_fluent_pos_rec via the
// per-checkpoint schedule built at plan-construction time (Phase 4c).
template<typename Callback>
void enumerate_condition_v2(const ConditionJoinPlan& plan,
                             const std::vector<fp::MutableAtom<f::FluentTag>>& src_atoms,
                             const SrcAtomsByPredicate* src_atoms_index,
                             const StaticAtomIndex& static_index,
                             const u::SubstitutionFunction<Data<f::Term>>& sigma,
                             Callback&& callback)
{
    enumerate_fluent_pos_rec(plan.positive_fluent, plan.negative_fluent, plan.negatives_at_checkpoint,
        plan.inequalities, plan.inequalities_at_checkpoint,
        0, src_atoms, src_atoms_index, sigma,
        [&](const u::SubstitutionFunction<Data<f::Term>>& sigma1)
        {
            join_static_v2(plan.static_join, 0, static_index, sigma1, std::forward<Callback>(callback));
        });
}

// Enumerate bindings for effect-only parameters from pattern atoms.
// Each unbound effect param is tried against each matching pattern atom position,
// plus a non-pattern option (leave unbound → ADD effects for this param are no-ops).
template<typename Callback>
void enumerate_effect_params_v2(const std::vector<EffectParamEnum>& enums,
                                size_t pos,
                                const std::vector<fp::MutableAtom<f::FluentTag>>& pattern_atoms,
                                const u::SubstitutionFunction<Data<f::Term>>& sigma,
                                Callback&& callback)
{
    if (pos == enums.size())
    {
        callback(sigma);
        return;
    }

    const auto& e = enums[pos];

    if (sigma.is_bound(e.param))
    {
        enumerate_effect_params_v2(enums, pos + 1, pattern_atoms, sigma, std::forward<Callback>(callback));
        return;
    }

    for (const auto& atom : pattern_atoms)
    {
        if (atom.predicate.get_index() != e.effect_pred.get_index())
            continue;
        if (e.arg_pos >= atom.terms.size() || !u::is_object(atom.terms[e.arg_pos]))
            continue;

        auto sigma2 = sigma;
        if (!sigma2.assign(e.param, Data<f::Term> { u::get_object(atom.terms[e.arg_pos]) }))
            continue;

        enumerate_effect_params_v2(enums, pos + 1, pattern_atoms, sigma2, callback);
    }

    // Non-pattern option: leave param unbound → non-ground ADD effects → no-op.
    enumerate_effect_params_v2(enums, pos + 1, pattern_atoms, sigma, std::forward<Callback>(callback));
}

// ---------------------------------------------------------------------------
// Old O(4^k) static literal satisfaction (kept for reference)
// ---------------------------------------------------------------------------

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

                                                                                            if (std::any_of(seen.begin(),
                                                                                                            seen.end(),
                                                                                                            [&](const auto& existing)
                                                                                                            {
                                                                                                                return EqualTo<
                                                                                                                    u::SubstitutionFunction<Index<f::Object>>> {}(
                                                                                                                    existing, *obj_sigma);
                                                                                                            }))
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

auto create_abstract_state_changing_transitions_v2(const std::vector<StateView<LiftedTag>>& astates,
                                                    const Pattern& pattern,
                                                    const ProjectionMapping<LiftedTag>::ActionMapping& projected_to_original_action,
                                                    const StaticAtomIndex& static_index,
                                                    const UnorderedMap<fp::ActionView, ActionJoinPlan>& join_plans,
                                                    const UnorderedMap<fp::ActionView, std::vector<std::size_t>>& param_domain_sizes_per_action,
                                                    const ReachableAtomIndex* reachable_index,
                                                    const ProjectionOptions& options)
{
    auto transitions = TransitionList {};
    auto adj_lists = std::vector<std::vector<uint_t>>(astates.size());

    const auto pattern_atoms = collect_pattern_atoms(pattern);
    const auto atom_bit_index = build_pattern_bit_index(pattern);

    // Per-action invariants that do not depend on the source state: the mutable
    // action copy, its base substitution, and the typed-domain sizes used by the
    // precondition post-check. Built once per action instead of once per
    // (src state, action) pair.
    struct PreAction
    {
        fp::MutableAction action;
        u::SubstitutionFunction<Data<f::Term>> sigma0;
        const std::vector<std::size_t>* pds;
    };
    static const std::vector<std::size_t> empty_pds {};
    auto pre_actions = UnorderedMap<fp::ActionView, PreAction> {};
    pre_actions.reserve(projected_to_original_action.size());
    for (const auto& [projected_action, info] : projected_to_original_action)
    {
        auto ma = fp::MutableAction(projected_action);
        auto s0 = make_sigma(ma);
        const auto pds_it = param_domain_sizes_per_action.find(projected_action);
        const auto* pds = (pds_it != param_domain_sizes_per_action.end()) ? &pds_it->second : &empty_pds;
        pre_actions.emplace(projected_action, PreAction { std::move(ma), std::move(s0), pds });
    }

    for (size_t src_idx = 0; src_idx < astates.size(); ++src_idx)
    {
        const auto& astate = astates[src_idx];
        const auto src_mask = uint_t(src_idx);
        const auto src_atoms = collect_visible_fluent_atoms(astate, pattern);

        // Phase 4b: build per-state fluent-atom index once, reused across all actions.
        // nullptr when the index is disabled (ablation path).
        std::optional<SrcAtomsByPredicate> src_index_storage;
        if (options.src_atoms_index == SrcAtomsIndex::On)
            src_index_storage = build_src_atoms_index(src_atoms);
        const SrcAtomsByPredicate* src_index_ptr = src_index_storage ? &(*src_index_storage) : nullptr;

        for (const auto& [projected_action, info] : projected_to_original_action)
        {
            const auto& join_plan = join_plans.at(projected_action);
            const auto& pre = pre_actions.at(projected_action);
            const auto& mutable_action = pre.action;
            const auto& sigma0 = pre.sigma0;

            auto seen = UnorderedSet<std::vector<std::uint32_t>> {};

            enumerate_condition_v2(join_plan.precondition, src_atoms, src_index_ptr, static_index, sigma0,
                [&](const u::SubstitutionFunction<Data<f::Term>>& sigma_pre)
                {
                    enumerate_effect_params_v2(join_plan.effect_param_enums, 0, pattern_atoms, sigma_pre,
                        [&](const u::SubstitutionFunction<Data<f::Term>>& sigma_full)
                        {
                            // After effect-side bindings, verify positive
                            // preconditions: drop transitions where a now-ground
                            // pattern-atom precondition is absent from src
                            // (basic post-check), or where a still-non-ground
                            // precondition has only pattern atoms as candidate
                            // groundings and none are in src (tightening).
                            if (!verify_pattern_preconditions(join_plan.precondition.positive_fluent,
                                                              sigma_full, pattern_atoms, atom_bit_index,
                                                              *pre.pds, reachable_index, src_mask))
                                return;

                            uint_t dst_mask = src_mask;

                            for (size_t ei = 0; ei < mutable_action.effects.size(); ++ei)
                            {
                                if (!join_plan.effects[ei].has_visible_effects)
                                    continue;

                                const auto& ceff = mutable_action.effects[ei];
                                const auto& ceff_plan = join_plan.effects[ei].condition_plan;

                                enumerate_condition_v2(ceff_plan, src_atoms, src_index_ptr, static_index, sigma_full,
                                    [&](const u::SubstitutionFunction<Data<f::Term>>& sigma_ceff)
                                    {
                                        for (const auto& lit : ceff.effect.literals)
                                            apply_effect_to_mask(lit, sigma_ceff, atom_bit_index, dst_mask);
                                    });
                            }

                            if (dst_mask == src_mask)
                                return;

                            const auto obj_sigma_opt = to_object_substitution(sigma_full, mutable_action.num_variables);
                            if (!obj_sigma_opt)
                                return;

                            if (!seen.insert(object_substitution_key(*obj_sigma_opt)).second)
                                return;

                            const auto sigma_original =
                                lift_substitution_to_original(*obj_sigma_opt, info.original_action.get_arity(), info.projected_to_original);

                            const auto t = uint_t(transitions.size());
                            transitions.push_back(Transition { projected_action, info.original_action, sigma_original, uint_t(src_idx), dst_mask });
                            adj_lists[src_idx].push_back(t);
                        });
                });
        }
    }

    return std::make_pair(std::move(transitions), std::move(adj_lists));
}

// Diagnostic: report projection-induced redundancy. For a given pattern, every
// transition we emit lands at exactly one (src, dst) pair under exactly one
// projected action. With pattern-irrelevant parameters in actions, many distinct
// ground-action enumerations can produce the same (src, dst, projected_action)
// triple — those are the "Source 2" redundancies that Phase 1–4 do not prune.
// We print per-action and per-pattern totals so we can tell from logs whether
// adding a regression-style projected-enumeration substrate would have a real
// ceiling on a given domain.
void emit_dedup_stats(size_t pattern_index, const TransitionList& transitions)
{
    using Pair = std::pair<uint_t, uint_t>;

    UnorderedMap<fp::ActionView, std::vector<Pair>> per_action;
    for (const auto& t : transitions)
        per_action[t.projected_action].push_back({ t.src, t.dst });

    size_t total_distinct_with_action = 0;
    for (auto& [action, pairs] : per_action)
    {
        const size_t emitted = pairs.size();
        std::sort(pairs.begin(), pairs.end());
        pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
        const size_t distinct = pairs.size();
        total_distinct_with_action += distinct;
        const double ratio = distinct == 0 ? 1.0 : static_cast<double>(emitted) / static_cast<double>(distinct);
        std::cout << "[DEDUP-STATS] pattern=" << pattern_index
                  << " action=" << action.get_name()
                  << " emitted=" << emitted
                  << " distinct_src_dst=" << distinct
                  << " redundancy=" << ratio
                  << std::endl;
    }

    auto all_pairs = std::vector<Pair> {};
    all_pairs.reserve(transitions.size());
    for (const auto& t : transitions)
        all_pairs.push_back({ t.src, t.dst });
    std::sort(all_pairs.begin(), all_pairs.end());
    all_pairs.erase(std::unique(all_pairs.begin(), all_pairs.end()), all_pairs.end());

    const size_t total_emitted = transitions.size();
    const size_t total_distinct = all_pairs.size();
    const double total_ratio = total_distinct == 0 ? 1.0 : static_cast<double>(total_emitted) / static_cast<double>(total_distinct);

    std::cout << "[DEDUP-STATS] pattern=" << pattern_index
              << " total_emitted=" << total_emitted
              << " total_distinct_src_dst=" << total_distinct
              << " total_distinct_src_dst_action=" << total_distinct_with_action
              << " redundancy=" << total_ratio
              << std::endl;
}

auto create_projection(const Pattern& pattern, size_t pattern_index, const Task<LiftedTag>& original_task,
                       const ReachableAtomIndex* reachable_index, const ProjectionOptions& options)
{
    auto [projected_task, projected_to_original_action] = project_task(original_task, pattern);

    // Build join plans once per projected task (Phase 1 precomputation).
    const auto static_index = build_static_atom_index(*projected_task);
    const auto join_plans = build_projection_join_plans(projected_to_original_action, pattern, static_index, options);

    // Pre-compute per-projected-action typed-domain sizes for parameters.
    // Used by `verify_pattern_preconditions`'s tightening pass: for a
    // non-ground positive precondition literal, the typed-domain product
    // over its unbound positions tells us how many ground candidates exist;
    // if that product equals the count of compatible pattern atoms, every
    // candidate is a pattern atom (so an absent-from-src check is exhaustive).
    // Ceff-local parameter slots get the sentinel `0` (unknown) so the
    // tightening conservatively skips them. Action-level params get their
    // precondition_domain.objects.size().
    auto param_domain_sizes_per_action = UnorderedMap<fp::ActionView, std::vector<std::size_t>> {};
    {
        const auto& var_domains = original_task.get_formalism_task().get_variable_domains_view();
        for (const auto& [projected_action, info] : projected_to_original_action)
        {
            const auto orig_it = var_domains.action_domains.find(info.original_action);
            if (orig_it == var_domains.action_domains.end())
                continue;
            const auto& orig_pre_doms = orig_it->second.payload.precondition_domain.payload;
            const auto orig_arity = info.original_action.get_arity();
            auto sizes = std::vector<std::size_t>(projected_action.get_arity(), 0);
            for (std::size_t i = 0; i < projected_action.get_arity(); ++i)
            {
                const auto orig_p = uint_t(info.projected_to_original[i]);
                if (orig_p < orig_arity && orig_p < orig_pre_doms.size())
                    sizes[i] = orig_pre_doms[orig_p].objects.size();
                // else ceff-local — leave 0 sentinel
            }
            param_domain_sizes_per_action.emplace(projected_action, std::move(sizes));
        }
    }

    auto state_repository = StateRepository<LiftedTag>::create(projected_task, ExecutionContext::create(1));

    auto [astates, goal_vertices] = create_abstract_states(pattern, *projected_task, *state_repository);
    auto [transitions, adj_lists] =
        create_abstract_state_changing_transitions_v2(astates, pattern, projected_to_original_action, static_index,
                                                       join_plans, param_domain_sizes_per_action, reachable_index, options);

    if (options.collect_dedup_stats)
        emit_dedup_stats(pattern_index, transitions);

    auto result = ProjectionAbstraction(std::make_shared<const ForwardProjectionAbstraction<LiftedTag>>(ProjectionMapping<LiftedTag>(pattern),
                                                                                                        std::move(state_repository),
                                                                                                        std::move(astates),
                                                                                                        std::move(transitions),
                                                                                                        std::move(adj_lists),
                                                                                                        std::move(goal_vertices)));

    return result;
}
}

ProjectionGenerator<LiftedTag>::ProjectionGenerator(std::shared_ptr<const Task<LiftedTag>> task, PatternCollection patterns, ProjectionOptions options) :
    m_task(std::move(task)),
    m_patterns(std::move(patterns)),
    m_options(options)
{
}

ProjectionAbstractionList<LiftedTag> ProjectionGenerator<LiftedTag>::generate()
{
    auto projections = ProjectionAbstractionList<LiftedTag> {};

    // Compute R+ once per task when the reachability filter is enabled.
    // R+ depends on the task, not on the pattern, so it's shared across all
    // projections in this generator instance.
    std::optional<ReachableAtomIndex> reachable_storage;
    if (m_options.reachability_filter)
    {
        // `RelaxedReachability<LiftedTag>` takes a non-const shared_ptr<Task>;
        // const_pointer_cast is safe because R+ only mutates an internal
        // Datalog workspace, not the task's logical state.
        auto task_nc = std::const_pointer_cast<Task<LiftedTag>>(m_task);
        auto exec_ctx = std::make_shared<ExecutionContext>(1);
        auto rr = RelaxedReachability<LiftedTag>(task_nc, exec_ctx);
        reachable_storage.emplace();
        for (const auto atom : rr.compute())
        {
            auto tup = std::vector<std::uint32_t> {};
            for (const auto obj : atom.get_row().get_objects())
                tup.push_back(static_cast<std::uint32_t>(uint_t(obj.get_index())));
            (*reachable_storage)[atom.get_predicate()].insert(std::move(tup));
        }
    }
    const ReachableAtomIndex* reachable_index = reachable_storage ? &(*reachable_storage) : nullptr;

    for (size_t i = 0; i < m_patterns.size(); ++i)
        projections.push_back(create_projection(m_patterns[i], i, *m_task, reachable_index, m_options));

    return projections;
}

}
