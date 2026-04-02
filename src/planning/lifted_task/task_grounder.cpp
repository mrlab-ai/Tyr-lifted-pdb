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

#include "tyr/planning/lifted_task/task_grounder.hpp"

#include "tyr/analysis/domains.hpp"
#include "tyr/common/dynamic_bitset.hpp"
#include "tyr/common/equal_to.hpp"
#include "tyr/common/hash.hpp"
#include "tyr/common/vector.hpp"
#include "tyr/datalog/bottom_up.hpp"
#include "tyr/datalog/contexts/program.hpp"
#include "tyr/datalog/policies/annotation.hpp"
#include "tyr/datalog/policies/termination.hpp"
#include "tyr/datalog/workspaces/program.hpp"
#include "tyr/formalism/canonicalization.hpp"
#include "tyr/formalism/planning/builder.hpp"
#include "tyr/formalism/planning/formatter.hpp"
#include "tyr/formalism/planning/grounder.hpp"
#include "tyr/formalism/planning/invariants/formatter.hpp"
#include "tyr/formalism/planning/invariants/mutexes.hpp"
#include "tyr/formalism/planning/invariants/synthesis.hpp"
#include "tyr/formalism/planning/merge.hpp"
#include "tyr/formalism/planning/merge_planning.hpp"
#include "tyr/formalism/planning/repository.hpp"
#include "tyr/formalism/planning/views.hpp"
#include "tyr/planning/applicability.hpp"
#include "tyr/planning/declarations.hpp"
#include "tyr/planning/ground_task.hpp"
#include "tyr/planning/lifted_task.hpp"
#include "tyr/planning/programs/ground.hpp"
#include "tyr/planning/task_utils.hpp"

namespace d = tyr::datalog;
namespace f = tyr::formalism;
namespace fp = tyr::formalism::planning;
namespace fpi = tyr::formalism::planning::invariant;

namespace tyr::planning
{

static auto remap_fdr_fact(fp::FDRFactView<f::FluentTag> fact, fp::FDRContext& fdr_context, fp::MergeContext& context)
{
    // Ensure that remapping is unambiguous
    assert(fact.get_variable().get_domain_size() == 2);

    auto new_atom = merge_p2p(fact.get_variable().get_atoms().front(), context).first.get_index();
    auto new_fact = fdr_context.get_fact(new_atom);

    // value 1 -> keep positive fact
    if (fact.get_value() != fp::FDRValue::none())
        return new_fact;

    // value 0 -> same variable, value 0
    new_fact.value = fp::FDRValue::none();

    return new_fact;
}

static auto create_ground_fdr_conjunctive_condition(fp::GroundConjunctiveConditionView element, fp::FDRContext& fdr_context, fp::MergeContext& context)
{
    auto fdr_conj_cond_ptr = context.builder.get_builder<fp::GroundConjunctiveCondition>();
    auto& fdr_conj_cond = *fdr_conj_cond_ptr;
    fdr_conj_cond.clear();

    for (const auto literal : element.get_facts<f::StaticTag>())
        fdr_conj_cond.static_literals.push_back(merge_p2p(literal, context).first.get_index());

    for (const auto fact : element.get_facts<f::FluentTag>())
        fdr_conj_cond.fluent_facts.push_back(remap_fdr_fact(fact, fdr_context, context));

    for (const auto literal : element.get_facts<f::DerivedTag>())
        fdr_conj_cond.derived_literals.push_back(merge_p2p(literal, context).first.get_index());

    for (const auto numeric_constraint : element.get_numeric_constraints())
        fdr_conj_cond.numeric_constraints.push_back(merge_p2p(numeric_constraint, context));

    canonicalize(fdr_conj_cond);
    return context.destination.get_or_create(fdr_conj_cond);
}

GroundTaskPtr ground_task(LiftedTask& lifted_task, ExecutionContext& execution_context, const GroundTaskOptions& options)
{
    /**
     * Execute datalog program.
     */

    auto ground_program = GroundTaskProgram(lifted_task.get_task());
    const auto const_workspace = d::ConstProgramWorkspace(ground_program.get_program_context());
    auto workspace = d::ProgramWorkspace<d::NoOrAnnotationPolicy, d::NoAndAnnotationPolicy, d::NoTerminationPolicy>(ground_program.get_program_context(),
                                                                                                                    const_workspace,
                                                                                                                    d::NoOrAnnotationPolicy(),
                                                                                                                    d::NoAndAnnotationPolicy(),
                                                                                                                    d::NoTerminationPolicy());
    auto ctx = d::ProgramExecutionContext(workspace, const_workspace);
    ctx.clear();

    execution_context.arena().execute([&] { d::solve_bottom_up(ctx); });
    workspace.d2p.clear();

    /**
     * Create basic structures of task
     */

    const auto& planning_task = lifted_task.get_formalism_task();
    const auto& planning_domain = planning_task.get_domain();

    auto task = planning_task.get_task();
    auto repository = planning_domain.get_repository_factory()->create_shared(planning_domain.get_repository().get());
    auto builder = fp::Builder();

    auto fdr_task_ptr = builder.get_builder<fp::FDRTask>();
    auto& fdr_task = *fdr_task_ptr;
    fdr_task.clear();

    auto merge_context = fp::MergeContext { builder, *repository };

    fdr_task.name = task.get_name();
    fdr_task.domain = task.get_domain().get_index();
    for (const auto predicate : task.get_derived_predicates())
        fdr_task.derived_predicates.push_back(merge_p2p(predicate, merge_context).first.get_index());
    for (const auto object : task.get_objects())
        fdr_task.objects.push_back(merge_p2p(object, merge_context).first.get_index());

    auto initial_atoms = fp::GroundAtomViewList<f::FluentTag> {};
    for (const auto atom : task.get_atoms<f::FluentTag>())
        initial_atoms.push_back(merge_p2p(atom, merge_context).first);

    /**
     * Create ground atoms
     */

    auto fluent_predicates = task.get_domain().get_predicates<f::FluentTag>();
    auto fluent_predicates_set = UnorderedSet<fp::PredicateView<f::FluentTag>>(fluent_predicates.begin(), fluent_predicates.end());
    auto fluent_atoms = fp::GroundAtomViewList<f::FluentTag> {};
    auto derived_predicates = task.get_domain().get_predicates<f::DerivedTag>();
    auto derived_predicates_set = UnorderedSet<fp::PredicateView<f::DerivedTag>>(derived_predicates.begin(), derived_predicates.end());
    auto derived_atoms = fp::GroundAtomViewList<f::DerivedTag> {};
    {
        auto merge_planning_context = fp::MergePlanningContext { builder, *repository };
        auto fluent_atom_ptr = builder.get_builder<fp::GroundAtom<f::FluentTag>>();
        auto& fluent_atom = *fluent_atom_ptr;
        auto derived_atom_ptr = builder.get_builder<fp::GroundAtom<f::DerivedTag>>();
        auto& derived_atom = *derived_atom_ptr;
        for (const auto& set : workspace.facts.fact_sets.predicate.get_sets())
        {
            if (ground_program.get_predicate_to_fluent_mapping().contains(set.get_predicate()))
            {
                for (const auto& binding : set.get_bindings())
                {
                    fluent_atom.clear();
                    fluent_atom.binding = fp::merge_d2p<f::FluentTag, f::FluentTag>(binding, merge_planning_context).first.get_index();
                    canonicalize(fluent_atom);
                    fluent_atoms.push_back(repository->get_or_create(fluent_atom).first);
                }
            }
            else if (ground_program.get_predicate_to_derived_mapping().contains(set.get_predicate()))
            {
                for (const auto& binding : set.get_bindings())
                {
                    derived_atom.clear();
                    derived_atom.binding = fp::merge_d2p<f::FluentTag, f::DerivedTag>(binding, merge_planning_context).first.get_index();
                    canonicalize(derived_atom);
                    derived_atoms.push_back(repository->get_or_create(derived_atom).first);
                }
            }
        }
    }
    std::sort(fluent_atoms.begin(), fluent_atoms.end());
    std::sort(derived_atoms.begin(), derived_atoms.end());

    std::cout << "Fluent atoms:" << std::endl;
    std::cout << fluent_atoms << std::endl;
    std::cout << "Derived atoms:" << std::endl;
    std::cout << derived_atoms << std::endl;

    auto fdr_context = std::shared_ptr<fp::FDRContext> { nullptr };

    if (options.enable_invariant_synthesis)
    {
        auto invariants = fpi::synthesize_invariants(planning_domain.get_domain());
        auto mutex_groups = fpi::compute_mutex_groups(initial_atoms, fluent_atoms, invariants);

        std::cout << "Invariants:" << std::endl;
        print(std::cout, invariants);
        std::cout << std::endl;
        std::cout << "Mutex groups:" << std::endl;
        print(std::cout, mutex_groups);
        std::cout << std::endl;

        fdr_context = std::make_shared<fp::FDRContext>(mutex_groups, repository);
    }
    else
    {
        fdr_context = std::make_shared<fp::FDRContext>(fluent_atoms, repository);
    }

    for (const auto atom : task.get_atoms<f::StaticTag>())
        fdr_task.static_atoms.push_back(merge_p2p(atom, merge_context).first.get_index());
    for (const auto atom : fluent_atoms)
        fdr_task.fluent_atoms.push_back(merge_p2p(atom, merge_context).first.get_index());

    // TODO: synthesize invariants, create mutex groups, instantiate fdr context

    for (const auto atom : derived_atoms)
        fdr_task.derived_atoms.push_back(merge_p2p(atom, merge_context).first.get_index());
    // for (const auto fterm : fluent_fterms)
    //     fdr_task.fluent_fterms.push_back(merge_p2p(fterm, merge_context).first.get_index());

    for (const auto fterm_value : task.get_fterm_values<f::StaticTag>())
        fdr_task.static_fterm_values.push_back(merge_p2p(fterm_value, merge_context).first.get_index());
    for (const auto fterm_value : task.get_fterm_values<f::FluentTag>())
        fdr_task.fluent_fterm_values.push_back(merge_p2p(fterm_value, merge_context).first.get_index());
    if (task.get_auxiliary_fterm_value().has_value())
        fdr_task.auxiliary_fterm_value = merge_p2p(task.get_auxiliary_fterm_value().value(), merge_context).first.get_index();
    if (task.get_metric())
        fdr_task.metric = merge_p2p(task.get_metric().value(), merge_context).first.get_index();
    for (const auto axiom : task.get_axioms())
        fdr_task.axioms.push_back(merge_p2p(axiom, merge_context).first.get_index());

    /// --- Create FDR variables
    for (const auto variable : fdr_context->get_variables())
        fdr_task.fluent_variables.push_back(variable.get_index());

    /// --- Create FDR fluent facts
    for (const auto atom : task.get_atoms<f::FluentTag>())
        fdr_task.fluent_facts.push_back(fdr_context->get_fact(merge_p2p(atom, merge_context).first.get_index()));

    /// --- Create FDR goal
    fdr_task.goal = create_ground_fdr_conjunctive_condition(task.get_goal(), *fdr_context, merge_context).first.get_index();

    /// --- Create FDR actions and axioms

    auto fluent_assign = UnorderedMap<Index<fp::FDRVariable<f::FluentTag>>, fp::FDRValue> {};
    auto derived_assign = UnorderedMap<Index<fp::GroundAtom<f::DerivedTag>>, bool> {};
    auto iter_workspace = itertools::cartesian_set::Workspace<Index<f::Object>> {};

    auto static_atoms_bitset = boost::dynamic_bitset<>();
    for (const auto atom : task.get_atoms<f::StaticTag>())
        set(uint_t(atom.get_index()), true, static_atoms_bitset);

    for (const auto& set : workspace.facts.fact_sets.predicate.get_sets())
    {
        for (const auto& binding : set.get_bindings())
        {
            const auto& mapping = ground_program.get_predicate_to_actions_mapping();

            if (const auto it = mapping.find(binding.get_relation()); it != mapping.end())
            {
                const auto action = it->second;

                workspace.d2p.binding.clear();
                for (const auto object : binding.get_objects())
                    workspace.d2p.binding.push_back(object.get_index());

                auto grounder_context = fp::GrounderContext { workspace.planning_builder, *repository, workspace.d2p.binding };

                const auto ground_action = fp::ground(action,
                                                      grounder_context,
                                                      lifted_task.get_parameter_domains_per_cond_effect_per_action()[uint_t(action.get_index())],
                                                      fluent_assign,
                                                      iter_workspace,
                                                      *fdr_context)
                                               .first;

                assert(is_statically_applicable(ground_action, static_atoms_bitset));

                if (is_consistent(ground_action, fluent_assign, derived_assign))
                {
                    fdr_task.ground_actions.push_back(ground_action.get_index());
                }
            }
        }
    }

    /// --- Ground Axioms

    auto ground_axioms_set = UnorderedSet<Index<fp::GroundAxiom>> {};

    for (const auto& set : workspace.facts.fact_sets.predicate.get_sets())
    {
        for (const auto& binding : set.get_bindings())
        {
            const auto& mapping = ground_program.get_predicate_to_axioms_mapping();

            if (const auto it = mapping.find(binding.get_relation()); it != mapping.end())
            {
                const auto axiom = it->second;

                workspace.d2p.binding.clear();
                for (const auto object : binding.get_objects())
                    workspace.d2p.binding.push_back(object.get_index());

                auto grounder_context = fp::GrounderContext { workspace.planning_builder, *repository, workspace.d2p.binding };

                const auto ground_axiom = fp::ground(axiom, grounder_context, *fdr_context).first;

                assert(is_statically_applicable(ground_axiom, static_atoms_bitset));

                if (is_consistent(ground_axiom, fluent_assign, derived_assign))
                {
                    fdr_task.ground_axioms.push_back(ground_axiom.get_index());
                }
            }
        }
    }

    canonicalize(fdr_task);

    return std::make_shared<GroundTask>(
        fp::PlanningFDRTask(repository->get_or_create(fdr_task).first, std::move(fdr_context), repository, planning_task.get_domain()));
}

}
