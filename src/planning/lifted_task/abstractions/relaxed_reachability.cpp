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

#include "tyr/planning/lifted_task/abstractions/relaxed_reachability.hpp"

#include "tyr/common/comparators.hpp"
#include "tyr/datalog/bottom_up.hpp"
#include "tyr/datalog/contexts/program.hpp"
#include "tyr/datalog/fact_sets.hpp"
#include "tyr/formalism/planning/merge_datalog.hpp"
#include "tyr/formalism/planning/merge_planning.hpp"
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/formalism/planning/views.hpp"

#include <algorithm>

namespace d = tyr::datalog;
namespace f = tyr::formalism;
namespace fp = tyr::formalism::planning;

namespace tyr::planning
{

RelaxedReachability<LiftedTag>::RelaxedReachability(std::shared_ptr<Task<LiftedTag>> task, ExecutionContextPtr execution_context) :
    m_task(std::move(task)),
    m_execution_context(std::move(execution_context)),
    m_workspace(m_task->get_rpg_program().get_program_context(),
                m_task->get_rpg_program().get_const_program_workspace(),
                d::NoOrAnnotationPolicy(),
                d::NoAndAnnotationPolicy(),
                d::NoTerminationPolicy())
{
}

std::shared_ptr<RelaxedReachability<LiftedTag>> RelaxedReachability<LiftedTag>::create(std::shared_ptr<Task<LiftedTag>> task,
                                                                                      ExecutionContextPtr execution_context)
{
    return std::make_shared<RelaxedReachability<LiftedTag>>(std::move(task), std::move(execution_context));
}

RelaxedReachability<LiftedTag>::ReachableAtomSet RelaxedReachability<LiftedTag>::compute()
{
    const auto& rpg_program = m_task->get_rpg_program();

    // Seed the workspace with the task's initial fluent atoms.
    auto merge_datalog_context = fp::MergeDatalogContext { m_workspace.datalog_builder, m_workspace.workspace_repository };

    m_workspace.facts.fact_sets.reset();
    m_workspace.facts.assignment_sets.reset();

    const auto planning_task = m_task->get_task();
    const auto& p2d = rpg_program.get_translation_context().p2d;

    for (const auto atom : planning_task.get_atoms<f::FluentTag>())
    {
        m_workspace.facts.fact_sets.predicate.insert(
            fp::merge_p2d<f::FluentTag, f::FluentTag>(atom, p2d.fluent_to_fluent_predicate, merge_datalog_context).first);
    }
    for (const auto fterm_value : planning_task.get_fterm_values<f::FluentTag>())
    {
        m_workspace.facts.fact_sets.function.insert(fp::merge_p2d(fterm_value, merge_datalog_context).first);
    }
    m_workspace.facts.assignment_sets.insert(m_workspace.facts.fact_sets);

    // Evaluate the delete-free RPG program to fixpoint.
    auto ctx = d::ProgramExecutionContext(m_workspace, rpg_program.get_const_program_workspace());
    ctx.clear();
    m_execution_context->arena().execute([&] { d::solve_bottom_up(ctx); });

    // Reverse-translate the fluent fact set back to planning ground atoms.
    auto merge_planning_context = fp::MergePlanningContext { m_workspace.planning_builder, *m_task->get_repository() };
    const auto& d2p = rpg_program.get_translation_context().d2p;

    auto reachable = ReachableAtomSet {};

    for (const auto& set : m_workspace.facts.fact_sets.predicate.get_sets())
    {
        if (!d2p.fluent_to_fluent_predicate.contains(set.get_predicate()))
            continue;

        for (const auto& binding : set.get_bindings())
        {
            const auto ground_atom =
                fp::merge_atom_d2p<f::FluentTag, f::FluentTag>(binding, d2p.fluent_to_fluent_predicate, merge_planning_context).first;
            reachable.insert(ground_atom);
        }
    }

    return reachable;
}

RelaxedReachability<LiftedTag>::ReachableAtomList RelaxedReachability<LiftedTag>::compute_sorted()
{
    auto set = compute();
    auto list = ReachableAtomList(set.begin(), set.end());
    std::sort(list.begin(), list.end(), tyr::Less<fp::GroundAtomView<f::FluentTag>> {});
    return list;
}

}
