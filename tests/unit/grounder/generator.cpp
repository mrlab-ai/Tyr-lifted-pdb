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
#include <oneapi/tbb/enumerable_thread_specific.h>
#include <oneapi/tbb/global_control.h>
#include <oneapi/tbb/parallel_for.h>
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

    /**
     * Preprocessing 1: Analyze variable domains
     */

    auto domains = analysis::compute_variable_domains(program);

    /**
     * Initialization 1: Execution contexts
     */

    // Per fact set
    auto facts_execution_context = grounder::FactsExecutionContext(program, domains);

    // Per rule
    auto rule_execution_contexts = std::vector<grounder::RuleExecutionContext> {};
    for (uint_t i = 0; i < program.get_rules().size(); ++i)
    {
        const auto rule = program.get_rules()[i];
        const auto& parameter_domains = domains.rule_domains[i];

        rule_execution_contexts.emplace_back(rule, parameter_domains, facts_execution_context.assignment_sets.get<formalism::StaticTag>(), repository);
    }

    // Per thread
    oneapi::tbb::enumerable_thread_specific<grounder::ThreadExecutionContext> thread_execution_contexts;

    /**
     * Parallelization 1: Lock-free rule grounding
     */

    const uint_t num_rules = program.get_rules().size();

    tbb::parallel_for(uint_t { 0 },
                      num_rules,
                      [&](uint_t i)
                      {
                          auto& rule_execution_context = rule_execution_contexts[i];
                          auto& thread_execution_context = thread_execution_contexts.local();  // thread-local

                          thread_execution_context.clear();

                          grounder::ground(facts_execution_context, rule_execution_context, thread_execution_context);
                      });

    /**
     * Parallelization 2: Lock-free hierarchical merging into global
     */

    auto hierarchical_merge_to_global_parallel = [&](std::vector<grounder::RuleExecutionContext>& recs, formalism::Repository& global_repo)
    {
        namespace tbb = oneapi::tbb;

        if (recs.empty())
            return;

        // Active indices into recs: start with all rules.
        std::vector<std::size_t> active(recs.size());
        std::iota(active.begin(), active.end(), std::size_t { 0 });

        // Helper: merge repo of recs[src_idx] into recs[dst_idx].
        auto merge_two = [&](std::size_t src_idx, std::size_t dst_idx)
        {
            auto& src = recs[src_idx];
            auto& dst = recs[dst_idx];

            auto& thread_execution_context = thread_execution_contexts.local();
            thread_execution_context.clear();

            for (const auto ground_rule_index : src.ground_rules)
            {
                auto ground_rule = View<Index<formalism::GroundRule>, formalism::OverlayRepository<Repository>>(ground_rule_index, src.repository);

                // Merge into dst.repository (hierarchical local repo)
                formalism::merge(ground_rule, thread_execution_context.builder, dst.repository, thread_execution_context.local_merge_cache);
            }

            // Optionally mark src as consumed
            src.ground_rules.clear();
        };

        // Tree-like reduction: repeatedly merge pairs in parallel until one survives.
        while (active.size() > 1)
        {
            const std::size_t n = active.size();
            const std::size_t num_pairs = n / 2;

            // Merge pairs (active[2k] -> active[2k+1]) in parallel
            tbb::parallel_for(std::size_t { 0 },
                              num_pairs,
                              [&](std::size_t k)
                              {
                                  auto i = active[2 * k];
                                  auto j = active[2 * k + 1];
                                  merge_two(i, j);
                              });

            // Build next active list: only the "right" side of each pair survives.
            std::vector<std::size_t> next;
            next.reserve((n + 1) / 2);

            for (std::size_t k = 0; k < num_pairs; ++k)
                next.push_back(active[2 * k + 1]);

            // If we had an odd number, the last one just carries over unchanged.
            if (n % 2 == 1)
                next.push_back(active.back());

            active.swap(next);
        }

        // Now we have a single "winner" repository left.
        auto winner_idx = active.front();
        auto& winner = recs[winner_idx];

        // Final merge: winner → global_repo (sequential)
        {
            auto& thread_execution_context = thread_execution_contexts.local();
            thread_execution_context.clear();

            for (const auto ground_rule_index : winner.ground_rules)
            {
                auto ground_rule = View<Index<formalism::GroundRule>, formalism::OverlayRepository<Repository>>(ground_rule_index, winner.repository);

                formalism::merge(ground_rule, thread_execution_context.builder, global_repo, thread_execution_context.global_merge_cache);
            }
        }
    };

    hierarchical_merge_to_global_parallel(rule_execution_contexts, repository);
}
}