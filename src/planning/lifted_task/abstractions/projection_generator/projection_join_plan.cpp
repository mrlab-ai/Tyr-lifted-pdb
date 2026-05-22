/*
 * Copyright (C) 2025-2026 Dominik Drexler
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

#include "projection_join_plan.hpp"

#include "tyr/formalism/unification/unification.hpp"
#include "tyr/planning/lifted_task.hpp"
#include "tyr/planning/lifted_task/state_view.hpp"

#include <cassert>
#include <limits>

namespace tyr::planning
{

namespace u = tyr::formalism::unification;

// ---------------------------------------------------------------------------
// StaticAtomIndex
// ---------------------------------------------------------------------------

void StaticAtomIndex::build(const Task<LiftedTag>& task)
{
    for (const auto atom : task.get_task().get_atoms<f::StaticTag>())
        by_predicate[atom.get_predicate()].emplace_back(atom);
}

const std::vector<fp::MutableAtom<f::StaticTag>>& StaticAtomIndex::lookup(fp::PredicateView<f::StaticTag> pred) const noexcept
{
    static const std::vector<fp::MutableAtom<f::StaticTag>> empty;
    const auto it = by_predicate.find(pred);
    return it != by_predicate.end() ? it->second : empty;
}

size_t StaticAtomIndex::count(fp::PredicateView<f::StaticTag> pred) const noexcept
{
    const auto it = by_predicate.find(pred);
    return it != by_predicate.end() ? it->second.size() : 0;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{

// Collect the set of parameter indices that appear in positive fluent literals.
// These are considered "initially bound" for the purpose of join ordering: the
// forward enumeration will match them against the abstract state before processing
// any static atoms.
std::vector<bool> collect_initially_bound(const fp::MutableLiteralList<f::FluentTag>& fluent_literals, size_t total_params)
{
    auto bound = std::vector<bool>(total_params, false);

    for (const auto& lit : fluent_literals)
    {
        if (!lit.polarity)
            continue;

        for (const auto& term : lit.atom.terms)
        {
            if (!u::is_parameter(term))
                continue;

            const auto idx = uint_t(u::get_parameter(term));
            if (idx < total_params)
                bound[idx] = true;
        }
    }

    return bound;
}

// Separate fluent literals into positive and negative lists.
std::pair<std::vector<fp::MutableLiteral<f::FluentTag>>, std::vector<fp::MutableLiteral<f::FluentTag>>>
split_fluent_literals(const fp::MutableLiteralList<f::FluentTag>& fluent_literals)
{
    auto pos = std::vector<fp::MutableLiteral<f::FluentTag>> {};
    auto neg = std::vector<fp::MutableLiteral<f::FluentTag>> {};

    for (const auto& lit : fluent_literals)
    {
        if (lit.polarity)
            pos.push_back(lit);
        else
            neg.push_back(lit);
    }

    return { std::move(pos), std::move(neg) };
}

// Build an ordered list of JoinSteps for the given static literals.
//
// Greedy ordering:
//   Repeatedly select the candidate that maximises the number of already-bound
//   parameters among its terms (using whatever the current binding frontier is),
//   breaking ties by choosing the candidate with the smallest estimated_size
//   (fewest ground atoms with that predicate).
//
// After ordering, each step records which of its parameters were already bound
// (constraining the lookup) and which it newly introduces.
std::vector<JoinStep> build_join_steps(const fp::MutableLiteralList<f::StaticTag>& literals,
                                       const StaticAtomIndex& static_index,
                                       std::vector<bool> bound)
{
    if (literals.empty())
        return {};

    struct Candidate
    {
        fp::MutableLiteral<f::StaticTag> literal;
        size_t estimated_size;
    };

    auto candidates = std::vector<Candidate> {};
    candidates.reserve(literals.size());
    for (const auto& lit : literals)
        candidates.push_back({ lit, static_index.count(lit.atom.predicate) });

    auto used = std::vector<bool>(candidates.size(), false);
    auto result = std::vector<JoinStep> {};
    result.reserve(candidates.size());

    for (size_t round = 0; round < candidates.size(); ++round)
    {
        size_t best_idx = std::numeric_limits<size_t>::max();
        size_t best_bound_count = 0;
        size_t best_size = std::numeric_limits<size_t>::max();

        for (size_t i = 0; i < candidates.size(); ++i)
        {
            if (used[i])
                continue;

            size_t bound_count = 0;
            for (const auto& term : candidates[i].literal.atom.terms)
            {
                if (!u::is_parameter(term))
                    continue;
                const auto idx = uint_t(u::get_parameter(term));
                if (idx < bound.size() && bound[idx])
                    ++bound_count;
            }

            const bool better = (bound_count > best_bound_count)
                                 || (bound_count == best_bound_count && candidates[i].estimated_size < best_size);

            if (better)
            {
                best_bound_count = bound_count;
                best_size = candidates[i].estimated_size;
                best_idx = i;
            }
        }

        assert(best_idx != std::numeric_limits<size_t>::max());
        used[best_idx] = true;

        const auto& cand = candidates[best_idx];
        auto step = JoinStep { cand.literal, {}, {}, cand.estimated_size };

        for (const auto& term : cand.literal.atom.terms)
        {
            if (!u::is_parameter(term))
                continue;

            const auto p = u::get_parameter(term);
            const auto idx = uint_t(p);

            if (idx < bound.size() && bound[idx])
                step.already_bound.push_back(p);
            else
            {
                step.newly_binds.push_back(p);
                if (idx < bound.size())
                    bound[idx] = true;
            }
        }

        result.push_back(std::move(step));
    }

    return result;
}

// Phase 4a: reorder positive fluent literals by selectivity.
//
// Greedy round-by-round: at each step pick the literal that maximises the
// already-bound parameter count, tie-breaking by (fewer remaining free params,
// smaller pattern_atom_count). After picking, mark its parameters as bound so
// later rounds see them.
//
// "Already bound" here tracks bindings within this fluent-literal sequence
// only; nothing carries over from elsewhere (the fluent stage runs before the
// static join in Phase 2).
std::vector<fp::MutableLiteral<f::FluentTag>>
order_positive_fluent_selectivity(std::vector<fp::MutableLiteral<f::FluentTag>> literals,
                                   const UnorderedMap<fp::PredicateView<f::FluentTag>, size_t>& pattern_atom_count,
                                   size_t total_params)
{
    if (literals.size() < 2)
        return literals;

    auto get_count = [&](fp::PredicateView<f::FluentTag> pred) -> size_t
    {
        const auto it = pattern_atom_count.find(pred);
        return it != pattern_atom_count.end() ? it->second : std::numeric_limits<size_t>::max();
    };

    auto bound = std::vector<bool>(total_params, false);
    auto used = std::vector<bool>(literals.size(), false);
    auto result = std::vector<fp::MutableLiteral<f::FluentTag>> {};
    result.reserve(literals.size());

    for (size_t round = 0; round < literals.size(); ++round)
    {
        size_t best_idx = std::numeric_limits<size_t>::max();
        size_t best_bound = 0;
        size_t best_free = std::numeric_limits<size_t>::max();
        size_t best_pcnt = std::numeric_limits<size_t>::max();

        for (size_t i = 0; i < literals.size(); ++i)
        {
            if (used[i])
                continue;

            size_t bound_count = 0;
            size_t free_params = 0;
            for (const auto& term : literals[i].atom.terms)
            {
                if (!u::is_parameter(term))
                    continue;
                const auto idx = uint_t(u::get_parameter(term));
                if (idx < bound.size() && bound[idx])
                    ++bound_count;
                else
                    ++free_params;
            }

            const size_t pcnt = get_count(literals[i].atom.predicate);

            const bool better =
                (best_idx == std::numeric_limits<size_t>::max())
                || (bound_count > best_bound)
                || (bound_count == best_bound && free_params < best_free)
                || (bound_count == best_bound && free_params == best_free && pcnt < best_pcnt);

            if (better)
            {
                best_idx = i;
                best_bound = bound_count;
                best_free = free_params;
                best_pcnt = pcnt;
            }
        }

        assert(best_idx != std::numeric_limits<size_t>::max());
        used[best_idx] = true;

        for (const auto& term : literals[best_idx].atom.terms)
        {
            if (!u::is_parameter(term))
                continue;
            const auto idx = uint_t(u::get_parameter(term));
            if (idx < bound.size())
                bound[idx] = true;
        }

        result.push_back(std::move(literals[best_idx]));
    }

    return result;
}

// Count distinct pattern atoms per predicate; used by Phase 4a ordering.
UnorderedMap<fp::PredicateView<f::FluentTag>, size_t>
build_pattern_predicate_counts(const Pattern& pattern)
{
    auto result = UnorderedMap<fp::PredicateView<f::FluentTag>, size_t> {};
    for (const auto atom : pattern.atoms_set)
    {
        const auto pred = fp::MutableAtom<f::FluentTag>(atom).predicate;
        result[pred] += 1;
    }
    return result;
}

// Build the ConditionJoinPlan for a single conjunctive condition.
// total_params: the total number of parameter slots visible in this scope
//   (for the main action condition this is action.num_variables;
//    for a ceff condition this is ceff.num_parent_variables + ceff.num_variables).
ConditionJoinPlan build_condition_join_plan(const fp::MutableConjunctiveCondition& condition,
                                            size_t total_params,
                                            const UnorderedMap<fp::PredicateView<f::FluentTag>, size_t>& pattern_atom_count,
                                            const StaticAtomIndex& static_index,
                                            const ProjectionOptions& options)
{
    auto [pos, neg] = split_fluent_literals(condition.fluent_literals);

    if (options.fluent_literal_order == FluentLiteralOrder::Selectivity)
        pos = order_positive_fluent_selectivity(std::move(pos), pattern_atom_count, total_params);

    auto initially_bound = collect_initially_bound(condition.fluent_literals, total_params);

    auto static_join = build_join_steps(condition.static_literals, static_index, std::move(initially_bound));

    return ConditionJoinPlan { std::move(pos), std::move(neg), std::move(static_join) };
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

StaticAtomIndex build_static_atom_index(const Task<LiftedTag>& task)
{
    StaticAtomIndex index;
    index.build(task);
    return index;
}

// Collect the set of parameter indices that are bound by the full precondition:
// both positive fluent literals AND static join steps.
static std::vector<bool> collect_precondition_bound(const ConditionJoinPlan& pre_plan, size_t total_params)
{
    // Start with fluent-bound params.
    std::vector<bool> bound(total_params, false);

    for (const auto& lit : pre_plan.positive_fluent)
    {
        for (const auto& term : lit.atom.terms)
        {
            if (!u::is_parameter(term))
                continue;
            const auto idx = uint_t(u::get_parameter(term));
            if (idx < total_params)
                bound[idx] = true;
        }
    }

    // Static join steps bind further params.
    for (const auto& step : pre_plan.static_join)
    {
        for (const auto p : step.newly_binds)
        {
            const auto idx = uint_t(p);
            if (idx < total_params)
                bound[idx] = true;
        }
    }

    return bound;
}

ActionJoinPlan build_action_join_plan(const fp::MutableAction& action,
                                      const Pattern& pattern,
                                      const StaticAtomIndex& static_index,
                                      const ProjectionOptions& options)
{
    ActionJoinPlan plan;

    const auto pattern_atom_count = build_pattern_predicate_counts(pattern);

    plan.precondition = build_condition_join_plan(action.condition, action.num_variables, pattern_atom_count, static_index, options);

    plan.effects.reserve(action.effects.size());
    for (const auto& ceff : action.effects)
    {
        const size_t total_params = ceff.num_parent_variables + ceff.num_variables;
        auto cond_plan = build_condition_join_plan(ceff.condition, total_params, pattern_atom_count, static_index, options);

        // In a projected action every effect literal is over a pattern predicate.
        const bool has_visible = !ceff.effect.literals.empty();

        plan.effects.push_back(EffectJoinInfo { std::move(cond_plan), has_visible });
    }

    // Collect params bound by the main precondition.
    const auto pre_bound = collect_precondition_bound(plan.precondition, action.num_variables);

    // Identify effect-only parameters: appear in effect literals but are NOT bound by precondition.
    // Record one EffectParamEnum per unique (param, effect_pred, arg_pos) triple.
    auto seen_params = std::vector<bool>(action.num_variables, false);

    for (const auto& ceff : action.effects)
    {
        for (const auto& lit : ceff.effect.literals)
        {
            for (size_t arg_pos = 0; arg_pos < lit.atom.terms.size(); ++arg_pos)
            {
                const auto& term = lit.atom.terms[arg_pos];
                if (!u::is_parameter(term))
                    continue;

                const auto idx = uint_t(u::get_parameter(term));
                if (idx >= action.num_variables)
                    continue;  // ceff-local variable; handled during ceff condition enum

                if (pre_bound[idx] || seen_params[idx])
                    continue;

                seen_params[idx] = true;
                plan.effect_param_enums.push_back(EffectParamEnum { u::get_parameter(term), lit.atom.predicate, arg_pos });
            }
        }
    }

    return plan;
}

UnorderedMap<fp::ActionView, ActionJoinPlan>
build_projection_join_plans(const ProjectionMapping<LiftedTag>::ActionMapping& projected_to_original_action,
                            const Pattern& pattern,
                            const StaticAtomIndex& static_index,
                            const ProjectionOptions& options)
{
    auto result = UnorderedMap<fp::ActionView, ActionJoinPlan> {};

    for (const auto& [projected_action, info] : projected_to_original_action)
    {
        const auto mutable_action = fp::MutableAction(projected_action);
        result.emplace(projected_action, build_action_join_plan(mutable_action, pattern, static_index, options));
    }

    return result;
}

}  // namespace tyr::planning
