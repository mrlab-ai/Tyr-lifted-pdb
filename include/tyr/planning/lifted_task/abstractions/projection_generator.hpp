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

#ifndef TYR_PLANNING_LIFTED_TASK_ABSTRACTIONS_PROJECTION_GENERATOR_HPP_
#define TYR_PLANNING_LIFTED_TASK_ABSTRACTIONS_PROJECTION_GENERATOR_HPP_

#include "tyr/common/declarations.hpp"
#include "tyr/formalism/planning/declarations.hpp"
#include "tyr/formalism/planning/fdr_fact_view.hpp"
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/planning/abstractions/explicit_projection.hpp"
#include "tyr/planning/abstractions/pattern_generator.hpp"
#include "tyr/planning/abstractions/projection_generator.hpp"
#include "tyr/planning/declarations.hpp"
#include "tyr/planning/lifted_task.hpp"

namespace f = tyr::formalism;
namespace fp = tyr::formalism::planning;

namespace tyr::planning
{

// Phase 4a: ordering strategy for positive fluent precondition literals during
// projection-transition enumeration. Selectivity is the optimized default.
enum class FluentLiteralOrder
{
    Declaration,
    Selectivity,
};

// Phase 4b: whether to build a per-src-state predicate-keyed index over visible
// fluent atoms (mirrors StaticAtomIndex on the fluent side). On is the optimized
// default.
enum class SrcAtomsIndex
{
    Off,
    On,
};

// Phase 4c: whether to push down negative fluent literal checks during positive
// enumeration. When On, each negative literal fires as soon as all its parameters
// become bound (instead of once at the end of positive enumeration). On is the
// optimized default.
enum class NegativeLiteralPushdown
{
    Off,
    On,
};

// Phase 4d: whether to specialise the handling of PDDL parameter inequality
// constraints (negative `=`-predicate literals like `(not (= ?x ?y))`). When On,
// inequalities are pulled out of the generic static_join and checked via direct
// object-identity comparison at the earliest checkpoint where both terms are
// ground — saving the O(|objects|) `contains_atom` scan over the reflexive `=`
// atom set. On is the optimized default.
enum class InequalityPropagation
{
    Off,
    On,
};

struct ProjectionOptions
{
    FluentLiteralOrder fluent_literal_order = FluentLiteralOrder::Selectivity;
    SrcAtomsIndex src_atoms_index = SrcAtomsIndex::On;
    NegativeLiteralPushdown negative_literal_pushdown = NegativeLiteralPushdown::On;
    InequalityPropagation inequality_propagation = InequalityPropagation::On;

    // Diagnostic-only: when true, after each projection is built, emit
    // [DEDUP-STATS] lines reporting how many transitions were emitted vs. how
    // many distinct (src, dst[, action]) edges those collapse to. Lets us
    // measure projection-induced redundancy (Source 2 in the Lauer-inspired
    // analysis) per pattern and per action. Default off — the stats add a
    // post-loop sort over the transition list per pattern.
    bool collect_dedup_stats = false;

    // When true, compute the delete-relaxation reachable fluent atoms (R+)
    // once per task and consult it during transition enumeration: drop
    // abstract transitions whose ground non-pattern positive precondition
    // atoms are not in R+. This emulates the operator-level reachability
    // filter Scorpion's grounder applies (a ground operator with an
    // unreachable precondition is dropped during grounding). Closes the
    // residual heuristic-strength gap from the 03-06-03 head-to-head where
    // a spurious `unload-airplane` transition fired under the abstraction
    // because its non-pattern preconditions `at(a0, l0-330)` and
    // `in(p4, a0)` were over-approximated as satisfied even though both
    // are unreachable from the initial state.
    bool reachability_filter = false;
};

template<>
class ProjectionGenerator<LiftedTag>
{
public:
    ProjectionGenerator(std::shared_ptr<const Task<LiftedTag>> task, PatternCollection patterns, ProjectionOptions options = {});

    ProjectionAbstractionList<LiftedTag> generate();

private:
    std::shared_ptr<const Task<LiftedTag>> m_task;
    PatternCollection m_patterns;
    ProjectionOptions m_options;
};

}

#endif
