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

#ifndef TYR_PLANNING_LIFTED_TASK_HPP_
#define TYR_PLANNING_LIFTED_TASK_HPP_

#include "tyr/common/config.hpp"          // for float_t, uint_t
#include "tyr/common/declarations.hpp"    // for UnorderedSet
#include "tyr/common/dynamic_bitset.hpp"  // for test
#include "tyr/common/onetbb.hpp"
#include "tyr/common/vector.hpp"                    // for get
#include "tyr/formalism/planning/declarations.hpp"  // for OverlayRepos...
#include "tyr/formalism/planning/fdr_context.hpp"
#include "tyr/formalism/planning/grounder_decl.hpp"
#include "tyr/formalism/planning/planning_task.hpp"
#include "tyr/formalism/planning/views.hpp"  // for View
#include "tyr/planning/declarations.hpp"
#include "tyr/planning/lifted_task/task_grounder_decl.hpp"
#include "tyr/planning/programs/action.hpp"
#include "tyr/planning/programs/axiom.hpp"
#include "tyr/planning/programs/rpg.hpp"

#include <boost/dynamic_bitset.hpp>  // for dynamic_bitset
#include <limits>                    // for numeric_limits
#include <memory>                    // for shared_ptr
#include <mutex>                     // for once_flag, call_once
#include <optional>                  // for optional
#include <vector>                    // for vector

namespace tyr::planning
{

template<>
class Task<LiftedTag>
{
public:
    explicit Task(formalism::planning::PlanningTask task);

    static std::shared_ptr<Task<LiftedTag>> create(formalism::planning::PlanningTask task);

    GroundTaskInstantiationResult instantiate_ground_task(ExecutionContext& execution_context,
                                                          const GroundTaskInstantiationOptions& options = GroundTaskInstantiationOptions());

    /**
     * Getters
     */

    const auto& get_formalism_task() const noexcept { return m_task; }
    const auto& get_domain() const noexcept { return m_task.get_domain(); }
    auto get_task() const noexcept { return m_task.get_task(); }
    auto& get_fdr_context() noexcept { return m_task.get_fdr_context(); }
    const auto& get_fdr_context() const noexcept { return m_task.get_fdr_context(); }
    const auto& get_repository() const noexcept { return m_task.get_repository(); }
    bool has_axioms() const noexcept { return !get_task().get_axioms().empty() || !get_domain().get_domain().get_axioms().empty(); }

    // The three datalog programs are constructed LAZILY on first access: tasks
    // created as per-pattern projections (ProjectionGenerator) never use them,
    // and building them eagerly re-merged the full static database into datalog
    // form once per pattern — dominant on static-heavy domains (genome).
    // std::call_once makes first access safe under the parallelized search.
    AxiomEvaluatorProgram& get_axiom_program() { return ensure_axiom_program(); }
    const AxiomEvaluatorProgram& get_axiom_program() const { return ensure_axiom_program(); }
    ApplicableActionProgram& get_action_program() { return ensure_action_program(); }
    const ApplicableActionProgram& get_action_program() const { return ensure_action_program(); }
    RPGProgram& get_rpg_program() { return ensure_rpg_program(); }
    const RPGProgram& get_rpg_program() const { return ensure_rpg_program(); }

    auto& get_grounder_cache() noexcept { return m_grounder_cache; }
    const auto& get_grounder_cache() const noexcept { return m_grounder_cache; }

    // Task-level memo for the delete-relaxation reachable fluent atoms (R+).
    // The fixpoint depends only on the task, but multiple components need it in
    // one run (pattern generation's reachability filter AND the projection
    // generator's reachability filter) — without the memo each recomputed the
    // identical fixpoint. The first caller computes via `fn`; later callers get
    // the cached set. call_once makes concurrent first access safe.
    using RelaxedReachableAtomSet = UnorderedSet<formalism::planning::GroundAtomView<formalism::FluentTag>>;

    template<typename ComputeFn>
    const RelaxedReachableAtomSet& get_or_compute_relaxed_reachable_atoms(ComputeFn&& fn) const
    {
        std::call_once(m_reachable_atoms_once, [&] { m_reachable_atoms = std::forward<ComputeFn>(fn)(); });
        return *m_reachable_atoms;
    }

    const auto& get_static_atoms_bitset() const noexcept { return m_static_atoms_bitset; }
    const auto& get_static_numeric_variables() const noexcept { return m_static_numeric_variables; }
    bool test(Index<formalism::planning::GroundAtom<formalism::StaticTag>> index) const { return tyr::test(uint_t(index), m_static_atoms_bitset); }
    float_t get(Index<formalism::planning::GroundFunctionTerm<formalism::StaticTag>> index) const
    {
        return tyr::get(uint_t(index), m_static_numeric_variables, std::numeric_limits<float_t>::quiet_NaN());
    }

private:
    AxiomEvaluatorProgram& ensure_axiom_program() const
    {
        std::call_once(m_axiom_program_once, [&] { m_axiom_program.emplace(get_task()); });
        return *m_axiom_program;
    }
    ApplicableActionProgram& ensure_action_program() const
    {
        std::call_once(m_action_program_once, [&] { m_action_program.emplace(get_task()); });
        return *m_action_program;
    }
    RPGProgram& ensure_rpg_program() const
    {
        std::call_once(m_rpg_program_once, [&] { m_rpg_program.emplace(get_task()); });
        return *m_rpg_program;
    }

    formalism::planning::PlanningTask m_task;

    boost::dynamic_bitset<> m_static_atoms_bitset;
    std::vector<float_t> m_static_numeric_variables;

    mutable std::optional<AxiomEvaluatorProgram> m_axiom_program;
    mutable std::once_flag m_axiom_program_once;

    mutable std::optional<ApplicableActionProgram> m_action_program;
    mutable std::once_flag m_action_program_once;

    mutable std::optional<RPGProgram> m_rpg_program;
    mutable std::once_flag m_rpg_program_once;

    mutable std::optional<RelaxedReachableAtomSet> m_reachable_atoms;
    mutable std::once_flag m_reachable_atoms_once;

    formalism::planning::GrounderCache m_grounder_cache;
};

}

#endif
