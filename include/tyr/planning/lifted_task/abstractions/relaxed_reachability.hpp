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

#ifndef TYR_PLANNING_LIFTED_TASK_ABSTRACTIONS_RELAXED_REACHABILITY_HPP_
#define TYR_PLANNING_LIFTED_TASK_ABSTRACTIONS_RELAXED_REACHABILITY_HPP_

#include "tyr/common/declarations.hpp"
#include "tyr/common/onetbb.hpp"
#include "tyr/common/unordered_set.hpp"
#include "tyr/datalog/policies/annotation.hpp"
#include "tyr/datalog/policies/termination.hpp"
#include "tyr/datalog/workspaces/program.hpp"
#include "tyr/formalism/planning/declarations.hpp"
#include "tyr/formalism/planning/views.hpp"
#include "tyr/planning/abstractions/relaxed_reachability.hpp"
#include "tyr/planning/declarations.hpp"
#include "tyr/planning/lifted_task.hpp"

#include <memory>
#include <vector>

namespace tyr::planning
{

template<>
class RelaxedReachability<LiftedTag>
{
public:
    using ReachableAtomSet = UnorderedSet<formalism::planning::GroundAtomView<formalism::FluentTag>>;
    using ReachableAtomList = std::vector<formalism::planning::GroundAtomView<formalism::FluentTag>>;

    explicit RelaxedReachability(std::shared_ptr<Task<LiftedTag>> task, ExecutionContextPtr execution_context);

    static std::shared_ptr<RelaxedReachability<LiftedTag>> create(std::shared_ptr<Task<LiftedTag>> task, ExecutionContextPtr execution_context);

    /// Compute the delete-relaxation reachable set of fluent ground atoms
    /// from the task's initial state. Runs the RPG-program's delete-free
    /// Datalog rules to fixpoint and reverse-translates the resulting
    /// fluent fact set back to planning ground atoms.
    ReachableAtomSet compute();

    /// Same as compute() but returns a deterministic, sorted list.
    ReachableAtomList compute_sorted();

private:
    std::shared_ptr<Task<LiftedTag>> m_task;
    ExecutionContextPtr m_execution_context;

    datalog::ProgramWorkspace<datalog::NoOrAnnotationPolicy, datalog::NoAndAnnotationPolicy, datalog::NoTerminationPolicy> m_workspace;
};

}

#endif
