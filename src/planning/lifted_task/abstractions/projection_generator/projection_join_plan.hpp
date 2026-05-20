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

#ifndef TYR_SRC_PLANNING_LIFTED_TASK_ABSTRACTIONS_PROJECTION_GENERATOR_PROJECTION_JOIN_PLAN_HPP_
#define TYR_SRC_PLANNING_LIFTED_TASK_ABSTRACTIONS_PROJECTION_GENERATOR_PROJECTION_JOIN_PLAN_HPP_

#include "tyr/common/declarations.hpp"
#include "tyr/formalism/planning/mutable/mutable.hpp"
#include "tyr/formalism/planning/views.hpp"
#include "tyr/planning/abstractions/explicit_projection.hpp"
#include "tyr/planning/abstractions/pattern_generator.hpp"
#include "tyr/planning/declarations.hpp"

namespace tyr::planning
{

namespace f = tyr::formalism;
namespace fp = tyr::formalism::planning;

/**
 * Index of ground static atoms grouped by predicate, built once per projected task.
 * Replaces the linear scan in satisfy_static_literals_rec with O(1) predicate lookup.
 */
struct StaticAtomIndex
{
    UnorderedMap<fp::PredicateView<f::StaticTag>, std::vector<fp::MutableAtom<f::StaticTag>>> by_predicate;

    void build(const Task<LiftedTag>& task);

    const std::vector<fp::MutableAtom<f::StaticTag>>& lookup(fp::PredicateView<f::StaticTag> pred) const noexcept;

    size_t count(fp::PredicateView<f::StaticTag> pred) const noexcept;
};

/**
 * One step in an ordered static join sequence.
 *
 * already_bound: action parameter indices that are bound before this step executes
 *   (from earlier join steps or from fluent preconditions). Used in Phase 2 to
 *   narrow the candidate tuples via constrained lookup.
 *
 * newly_binds: parameter indices that this step uniquely determines.
 *
 * estimated_size: |static_index[predicate]| — used to order steps so the most
 *   selective atoms come first.
 */
struct JoinStep
{
    fp::MutableLiteral<f::StaticTag> literal;
    std::vector<f::ParameterIndex> already_bound;
    std::vector<f::ParameterIndex> newly_binds;
    size_t estimated_size;
};

/**
 * Precomputed join plan for one conjunctive condition (either the action's main
 * precondition or a conditional-effect condition).
 */
struct ConditionJoinPlan
{
    // Positive fluent literals — matched against the abstract source state.
    std::vector<fp::MutableLiteral<f::FluentTag>> positive_fluent;
    // Negative fluent literals — verified absent in the abstract source state.
    std::vector<fp::MutableLiteral<f::FluentTag>> negative_fluent;
    // Static literals in greedy join order (most selective / most constrained first).
    std::vector<JoinStep> static_join;
};

/**
 * Per-conditional-effect information.
 */
struct EffectJoinInfo
{
    ConditionJoinPlan condition_plan;
    // True iff the effect has visible literals (always true in projected actions).
    bool has_visible_effects;
};

/**
 * Descriptor for a parameter that does not appear in any precondition atom
 * (neither fluent nor static) and must therefore be enumerated from the pattern
 * atoms of the effect predicate it appears in.
 *
 * During forward transition enumeration (Phase 2), after the precondition join
 * has bound all precondition-visible parameters, any remaining unbound parameters
 * of this kind are enumerated by scanning the k pattern atoms for the given
 * predicate.  This keeps the enumeration bounded by the pattern size.
 */
struct EffectParamEnum
{
    f::ParameterIndex param;
    fp::PredicateView<f::FluentTag> effect_pred;  // which effect predicate uses this param
    size_t arg_pos;                               // argument position within that predicate
};

/**
 * All precomputed join data for one projected action schema.
 * Built once per (pattern, action) pair during ProjectionGenerator::generate().
 */
struct ActionJoinPlan
{
    ConditionJoinPlan precondition;
    std::vector<EffectJoinInfo> effects;
    // Parameters not constrained by any precondition atom; enumerated from
    // pattern atoms of their respective effect predicates.
    std::vector<EffectParamEnum> effect_param_enums;
};

/**
 * Build the static atom index for a projected task.
 * Must be called after project_task() so the task's static atoms are populated.
 */
StaticAtomIndex build_static_atom_index(const Task<LiftedTag>& task);

/**
 * Build the join plan for one projected action schema.
 * The greedy ordering maximises the number of already-bound parameters at each step,
 * breaking ties by estimated_size (fewer tuples first).
 */
ActionJoinPlan build_action_join_plan(const fp::MutableAction& action,
                                      const Pattern& pattern,
                                      const StaticAtomIndex& static_index);

/**
 * Build join plans for every projected action in a projection's action mapping.
 */
UnorderedMap<fp::ActionView, ActionJoinPlan>
build_projection_join_plans(const ProjectionMapping<LiftedTag>::ActionMapping& projected_to_original_action,
                            const Pattern& pattern,
                            const StaticAtomIndex& static_index);

}  // namespace tyr::planning

#endif
