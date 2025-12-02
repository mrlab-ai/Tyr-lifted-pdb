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

#include "../utils.hpp"

#include <gtest/gtest.h>
#include <tyr/analysis/analysis.hpp>
#include <tyr/grounder/grounder.hpp>

using namespace tyr::buffer;
using namespace tyr::formalism;

namespace tyr::tests
{

TEST(TyrTests, TyrGrounderGenerator)
{
    auto [program_index, repository] = create_example_problem();
    auto program = View<Index<Program>, Repository>(program_index, repository);

    std::cout << program << std::endl;

    // Once per program: Analyze variable domains to compress assignment sets
    auto domains = analysis::compute_variable_domains(program);

    auto facts_execution_context = grounder::FactsExecutionContext(program, domains);

    auto rule_execution_contexts = std::vector<grounder::RuleExecutionContext> {};
    for (uint_t i = 0; i < program.get_rules().size(); ++i)
    {
        const auto rule = program.get_rules()[i];
        const auto& parameter_domains = domains.rule_domains[i];

        rule_execution_contexts.emplace_back(rule, parameter_domains, facts_execution_context.assignment_sets.get<formalism::StaticTag>(), repository);
    }

    auto thread_execution_contexts = std::vector<grounder::ThreadExecutionContext>(1);  // Use 1 context for now

    // Per fact set: Create workspaces that wrap all the data for grounding, then call ground
    // TODO: Use onetbb parallel for loop.
    for (uint_t i = 0; i < program.get_rules().size(); ++i)
    {
        std::cout << "r: " << program.get_rules()[i] << std::endl;

        // Combine all the data dependencies into workspaces.
        auto& rule_execution_context = rule_execution_contexts[i];

        auto& thread_execution_context = thread_execution_contexts[0];

        grounder::ground(facts_execution_context, rule_execution_context, thread_execution_context);
    }

    // Merge the ScopeRepositories into the global one
    // TODO: Use ontbb parallel for loop and merge in log_2(num rules) depth.

    auto merge_cache = formalism::MergeCache<formalism::OverlayRepository<Repository>, Repository> {};

    for (uint_t i = 0; i < program.get_rules().size(); ++i)
    {
        auto& rule_execution_context = rule_execution_contexts[i];

        auto& thread_execution_context = thread_execution_contexts[0];

        for (const auto ground_rule_index : rule_execution_context.ground_rules)
        {
            auto ground_rule =
                View<Index<formalism::GroundRule>, formalism::OverlayRepository<Repository>>(ground_rule_index, rule_execution_context.repository);
            formalism::merge(ground_rule, thread_execution_context.builder, repository, merge_cache);
        }
    }
}
}