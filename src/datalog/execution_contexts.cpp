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

#include "tyr/datalog/execution_contexts.hpp"

#include "tyr/formalism/datalog/arity.hpp"
#include "tyr/formalism/datalog/formatter.hpp"
#include "tyr/formalism/datalog/grounder.hpp"

namespace f = tyr::formalism;
namespace fd = tyr::formalism::datalog;

namespace tyr::datalog
{
/**
 * FactsExecutionContext
 */

FactsExecutionContext::FactsExecutionContext(View<Index<fd::Program>, fd::Repository> program, const analysis::ProgramVariableDomains& domains) :
    fact_sets(program),
    assignment_sets(program, domains, fact_sets)
{
}

FactsExecutionContext::FactsExecutionContext(View<Index<fd::Program>, fd::Repository> program,
                                             TaggedFactSets<f::FluentTag> fluent_facts,
                                             const analysis::ProgramVariableDomains& domains) :
    fact_sets(program, fluent_facts),
    assignment_sets(program, domains, fact_sets)
{
}

template<f::FactKind T>
void FactsExecutionContext::reset() noexcept
{
    fact_sets.template reset<T>();
    assignment_sets.template reset<T>();
}

template void FactsExecutionContext::reset<f::StaticTag>() noexcept;
template void FactsExecutionContext::reset<f::FluentTag>() noexcept;

void FactsExecutionContext::reset() noexcept
{
    fact_sets.reset();
    assignment_sets.reset();
}

template<f::FactKind T>
void FactsExecutionContext::insert(View<IndexList<fd::GroundAtom<T>>, fd::Repository> view)
{
    fact_sets.insert(view);
    assignment_sets.insert(fact_sets.template get<T>());
}

template void FactsExecutionContext::insert(View<IndexList<fd::GroundAtom<f::StaticTag>>, fd::Repository> view);
template void FactsExecutionContext::insert(View<IndexList<fd::GroundAtom<f::FluentTag>>, fd::Repository> view);

template<f::FactKind T>
void FactsExecutionContext::insert(View<IndexList<fd::GroundFunctionTermValue<T>>, fd::Repository> view)
{
    fact_sets.insert(view);
    assignment_sets.insert(fact_sets.template get<T>());
}

template void FactsExecutionContext::insert(View<IndexList<fd::GroundFunctionTermValue<f::StaticTag>>, fd::Repository> view);
template void FactsExecutionContext::insert(View<IndexList<fd::GroundFunctionTermValue<f::FluentTag>>, fd::Repository> view);

/**
 * RuleStageExecutionContext
 */

RuleStageExecutionContext::RuleStageExecutionContext() : repository(std::make_shared<fd::Repository>()), binding(), ground_heads(), merge_cache() {}

void RuleStageExecutionContext::clear() noexcept
{
    repository->clear();
    ground_heads.clear();
    merge_cache.clear();
}

/**
 * StaticRuleExecutionContext
 */

StaticRuleExecutionContext StaticRuleExecutionContext::create(View<Index<fd::Rule>, fd::Repository> rule,
                                                              fd::Repository& repository,
                                                              const analysis::DomainListList& parameter_domains,
                                                              const TaggedAssignmentSets<f::StaticTag>& static_assignment_sets)
{
    auto builder = fd::Builder();

    auto nullary_condition = make_view(create_ground_nullary_condition(rule.get_body(), builder, repository).first, repository);

    auto unary_overapproximation_condition =
        make_view(create_overapproximation_conjunctive_condition(1, rule.get_body(), builder, repository).first, repository);
    auto binary_overapproximation_condition =
        make_view(create_overapproximation_conjunctive_condition(2, rule.get_body(), builder, repository).first, repository);

    auto unary_conflicting_overapproximation_condition =
        make_view(create_overapproximation_conflicting_conjunctive_condition(1, rule.get_body(), builder, repository).first, repository);
    auto binary_conflicting_overapproximation_condition =
        make_view(create_overapproximation_conflicting_conjunctive_condition(2, rule.get_body(), builder, repository).first, repository);

    auto static_consistency_graph = StaticConsistencyGraph(rule.get_body(),
                                                           unary_overapproximation_condition,
                                                           binary_overapproximation_condition,
                                                           parameter_domains,
                                                           0,
                                                           rule.get_arity(),
                                                           static_assignment_sets);

    return StaticRuleExecutionContext { rule,
                                        nullary_condition,
                                        unary_overapproximation_condition,
                                        binary_overapproximation_condition,
                                        unary_conflicting_overapproximation_condition,
                                        binary_conflicting_overapproximation_condition,
                                        std::move(static_consistency_graph) };
}

/**
 * RuleExecutionContext
 */

RuleExecutionContext::RuleExecutionContext(View<Index<fd::Rule>, fd::Repository> rule,
                                           View<Index<fd::GroundConjunctiveCondition>, fd::Repository> nullary_condition,
                                           View<Index<fd::ConjunctiveCondition>, fd::Repository> unary_overapproximation_condition,
                                           View<Index<fd::ConjunctiveCondition>, fd::Repository> binary_overapproximation_condition,
                                           View<Index<fd::ConjunctiveCondition>, fd::Repository> unary_conflicting_overapproximation_condition,
                                           View<Index<fd::ConjunctiveCondition>, fd::Repository> binary_conflicting_overapproximation_condition,
                                           const analysis::DomainListList& parameter_domains,
                                           const TaggedAssignmentSets<f::StaticTag>& static_assignment_sets,
                                           const fd::Repository& parent) :
    rule(rule),
    nullary_condition(nullary_condition),
    unary_overapproximation_condition(unary_overapproximation_condition),
    binary_overapproximation_condition(binary_overapproximation_condition),
    unary_conflicting_overapproximation_condition(unary_conflicting_overapproximation_condition),
    binary_conflicting_overapproximation_condition(binary_conflicting_overapproximation_condition),
    static_consistency_graph(rule.get_body(),
                             unary_overapproximation_condition,
                             binary_overapproximation_condition,
                             parameter_domains,
                             0,
                             rule.get_arity(),
                             static_assignment_sets),
    consistency_graph(datalog::kpkc::allocate_dense_graph(static_consistency_graph)),
    kpkc_workspace(datalog::kpkc::allocate_workspace(static_consistency_graph)),
    repository(std::make_shared<fd::Repository>()),  // we have to use pointer, since the RuleExecutionContext is moved into a vector
    overlay_repository(parent, *repository),
    binding(),
    ground_heads()
{
}

void RuleExecutionContext::clear() noexcept
{
    repository->clear();
    ground_heads.clear();
}

void RuleExecutionContext::initialize(const AssignmentSets& assignment_sets)
{
    datalog::kpkc::initialize_dense_graph_and_workspace(static_consistency_graph, assignment_sets, consistency_graph, kpkc_workspace);
}

/**
 * ThreadExecutionContext
 */

void ThreadExecutionContext::clear() noexcept { merge_cache.clear(); }

/**
 * ProgramToTaskExecutionContext
 */

void ProgramToTaskExecutionContext::clear() noexcept { merge_cache.clear(); }

/**
 * TaskToProgramExecutionContext
 */

void TaskToProgramExecutionContext::clear() noexcept { merge_cache.clear(); }

/**
 * ProgramExecutionContext
 */

ProgramExecutionContext::ProgramExecutionContext(View<Index<fd::Program>, fd::Repository> program,
                                                 fd::RepositoryPtr repository,
                                                 const analysis::ProgramVariableDomains& domains,
                                                 const analysis::RuleStrata& strata,
                                                 const analysis::ListenerStrata& listeners) :
    program(program),
    repository(repository),
    domains(domains),
    strata(strata),
    listeners(listeners),
    rule_scheduler_strata(create_rule_scheduler_strata(strata, listeners, *repository)),
    planning_builder(),
    datalog_builder(),
    facts_execution_context(program, domains),
    rule_execution_contexts(),
    rule_stage_execution_contexts(),
    thread_execution_contexts(),
    program_to_task_execution_context(),
    task_to_program_execution_context()
{
    for (uint_t i = 0; i < program.get_rules().size(); ++i)
    {
        rule_execution_contexts.emplace_back(
            program.get_rules()[i],
            make_view(create_ground_nullary_condition(program.get_rules()[i].get_body(), datalog_builder, *repository).first, *repository),
            make_view(create_overapproximation_conjunctive_condition(1, program.get_rules()[i].get_body(), datalog_builder, *repository).first, *repository),
            make_view(create_overapproximation_conjunctive_condition(2, program.get_rules()[i].get_body(), datalog_builder, *repository).first, *repository),
            make_view(create_overapproximation_conflicting_conjunctive_condition(1, program.get_rules()[i].get_body(), datalog_builder, *repository).first,
                      *repository),
            make_view(create_overapproximation_conflicting_conjunctive_condition(2, program.get_rules()[i].get_body(), datalog_builder, *repository).first,
                      *repository),
            domains.rule_domains[i],
            facts_execution_context.assignment_sets.static_sets,
            *repository);
        rule_execution_contexts.back().initialize(facts_execution_context.assignment_sets);
    }
    rule_stage_execution_contexts.resize(rule_execution_contexts.size());
}

}
