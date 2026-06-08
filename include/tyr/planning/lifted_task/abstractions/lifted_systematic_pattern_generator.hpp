/*
 * Copyright (C) 2026 Dominik Drexler
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

#ifndef TYR_PLANNING_LIFTED_TASK_ABSTRACTIONS_LIFTED_SYSTEMATIC_PATTERN_GENERATOR_HPP_
#define TYR_PLANNING_LIFTED_TASK_ABSTRACTIONS_LIFTED_SYSTEMATIC_PATTERN_GENERATOR_HPP_

#include "tyr/common/declarations.hpp"
#include "tyr/formalism/planning/declarations.hpp"
#include "tyr/planning/abstractions/pattern_generator.hpp"
#include "tyr/planning/declarations.hpp"
#include "tyr/planning/lifted_task.hpp"

#include <cstddef>
#include <memory>

namespace tyr::planning
{

/// Knobs mirroring `python/prototypes/pattern_gen.py:LiftedPatternGenerator.__init__`.
/// Defaults match the Python defaults.
struct LiftedSystematicPatternGeneratorOptions
{
    bool bounded_fallback = true;
    bool static_csp = true;
    bool reachability = true;
    bool scorpion_match = false;
    /// When true, augment the SGA output with the sys2 co-effect
    /// disjoint-union step (lifted equivalent of Scorpion's
    /// `pattern_type=interesting`). Builds an `(eff_pred, eff_pred)` →
    /// co-effect-edge table and emits goal-pair patterns where at least
    /// one binding makes both atoms positive effects of the same action.
    bool interesting = false;

    std::size_t max_pattern_size = 2;
    std::size_t max_pattern_count = 10;
};

/// Native port of `python/prototypes/pattern_gen.py:LiftedPatternGenerator`.
///
/// Generates patterns of size ≤ `max_pattern_size` by BFS expansion from
/// goal facts along the predicate-level causal graph, with optional static-
/// CSP and delete-relaxation-reachability filters. Lifted-only — the
/// algorithm relies on action schemas and lifted preconditions; the ground
/// case is covered by `GoalPatternGenerator<GroundTag>`.
class LiftedSystematicPatternGenerator : public PatternGenerator<LiftedTag>
{
public:
    LiftedSystematicPatternGenerator(std::shared_ptr<const Task<LiftedTag>> task, LiftedSystematicPatternGeneratorOptions options);
    ~LiftedSystematicPatternGenerator();

    LiftedSystematicPatternGenerator(const LiftedSystematicPatternGenerator&) = delete;
    LiftedSystematicPatternGenerator& operator=(const LiftedSystematicPatternGenerator&) = delete;
    LiftedSystematicPatternGenerator(LiftedSystematicPatternGenerator&&) = delete;
    LiftedSystematicPatternGenerator& operator=(LiftedSystematicPatternGenerator&&) = delete;

    static std::shared_ptr<LiftedSystematicPatternGenerator>
    create(std::shared_ptr<const Task<LiftedTag>> task, LiftedSystematicPatternGeneratorOptions options);

    /// `PatternGenerator<LiftedTag>` interface — uses the limits stored
    /// in `m_options`.
    PatternCollection generate() override;

    /// Number of (eff_pred, pre_pred) action edges built from the Phase 6.6
    /// causal-graph pass. Exposed for parity testing against the Python
    /// reference implementation.
    std::size_t num_action_edges() const noexcept;

private:
    struct Impl;
    std::shared_ptr<const Task<LiftedTag>> m_task;
    LiftedSystematicPatternGeneratorOptions m_options;
    std::unique_ptr<Impl> m_impl;
};

}

#endif
