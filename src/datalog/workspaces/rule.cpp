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

#include "tyr/datalog/workspaces/rule.hpp"

#include "tyr/datalog/assignment_sets.hpp"
#include "tyr/datalog/kpkc_utils.hpp"
#include "tyr/formalism/datalog/rule_view.hpp"

#include <chrono>
#include <vector>

namespace fd = tyr::formalism::datalog;

namespace tyr::datalog
{
/**
 * RuleWorkspace
 */

RuleWorkspace::RuleWorkspace(const formalism::datalog::Repository& parent, const StaticConsistencyGraph& static_consistency_graph) :
    consistency_graph(datalog::kpkc::allocate_dense_graph(static_consistency_graph)),
    kpkc_workspace(datalog::kpkc::allocate_workspace(static_consistency_graph)),
    repository(std::make_shared<fd::Repository>()),  // we have to use pointer, since the RuleExecutionContext is moved into a vector
    overlay_repository(parent, *repository),
    binding(),
    ground_heads(),
    statistics()
{
}

void RuleWorkspace::clear() noexcept
{
    repository->clear();
    ground_heads.clear();
}

void RuleWorkspace::initialize(const StaticConsistencyGraph& static_consistency_graph, const AssignmentSets& assignment_sets)
{
    datalog::kpkc::initialize_dense_graph_and_workspace(static_consistency_graph, assignment_sets, consistency_graph, kpkc_workspace);
}

/**
 * ConstRuleWorkspace
 */

ConstRuleWorkspace::ConstRuleWorkspace(Index<formalism::datalog::Rule> rule,
                                       formalism::datalog::Repository& repository,
                                       const analysis::DomainListList& parameter_domains,
                                       const TaggedAssignmentSets<formalism::StaticTag>& static_assignment_sets) :
    rule(rule),
    repository(repository),
    nullary_condition(create_ground_nullary_condition(get_rule().get_body(), repository).first),
    unary_overapproximation_condition(create_overapproximation_conjunctive_condition(1, get_rule().get_body(), repository).first),
    binary_overapproximation_condition(create_overapproximation_conjunctive_condition(2, get_rule().get_body(), repository).first),
    unary_conflicting_overapproximation_condition(create_overapproximation_conflicting_conjunctive_condition(1, get_rule().get_body(), repository).first),
    binary_conflicting_overapproximation_condition(create_overapproximation_conflicting_conjunctive_condition(2, get_rule().get_body(), repository).first),
    static_consistency_graph(get_rule().get_body(),
                             get_unary_overapproximation_condition(),
                             get_binary_overapproximation_condition(),
                             parameter_domains,
                             0,
                             get_rule().get_arity(),
                             static_assignment_sets)
{
}

}
