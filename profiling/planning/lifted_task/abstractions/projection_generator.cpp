/*
 * Copyright (C) 2026 Dominik Drexler and Jordan Thayer
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

#include "tyr/planning/lifted_task/abstractions/projection_generator.hpp"

#include "tyr/common/json_loader.hpp"
#include "tyr/formalism/planning/parser.hpp"
#include "tyr/planning/abstractions/goal_pattern_generator.hpp"
#include "tyr/planning/heuristics/canonical.hpp"
#include "tyr/planning/lifted_task.hpp"
#include "tyr/planning/lifted_task/state_repository.hpp"

#include <benchmark/benchmark.h>
#include <boost/json.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace fp = tyr::formalism::planning;
namespace p = tyr::planning;
namespace json = boost::json;

namespace
{
struct BenchmarkCase
{
    std::string name;
    std::filesystem::path domain;
    std::filesystem::path task;
};

std::vector<BenchmarkCase> load_cases()
{
    const auto document = tyr::common::load_json_file(tyr::common::profiling_path("planning/lifted_task/abstractions/projection_generator.json"));
    const auto& root = document.as_object();
    const auto& domains = root.at("domains").as_object();

    auto result = std::vector<BenchmarkCase>();

    for (const auto& [domain_name_key, domain_value] : domains)
    {
        const auto& domain_object = domain_value.as_object();
        const auto domain_name = std::string(domain_name_key);
        const auto domain = tyr::common::data_path(json::value_to<std::string>(domain_object.at("domain_file")));
        const auto& tasks = domain_object.at("tasks").as_object();

        for (const auto& [task_name_key, task_value] : tasks)
        {
            const auto task_name = std::string(task_name_key);
            const auto run_name = domain_name + "/" + task_name;
            const auto task = tyr::common::data_path(json::value_to<std::string>(task_value));

            result.push_back(BenchmarkCase { run_name, domain, task });
        }
    }

    return result;
}

p::LiftedTaskPtr create_task(const BenchmarkCase& benchmark_case)
{
    return p::LiftedTask::create(fp::Parser(benchmark_case.domain).parse_task(benchmark_case.task));
}

void benchmark_projection_generator(benchmark::State& state, const BenchmarkCase& benchmark_case)
{
    auto task = create_task(benchmark_case);
    auto patterns = p::GoalPatternGenerator<p::LiftedTag>(task).generate();
    auto projections = std::vector<p::ProjectionAbstraction<p::LiftedTag>>();

    for (auto _ : state)
    {
        p::reset_projection_generator_instrumentation();
        projections = p::ProjectionGenerator<p::LiftedTag>(task, patterns).generate();

        benchmark::DoNotOptimize(projections);
    }

    auto num_transitions = size_t { 0 };
    for (const auto& projection : projections)
        num_transitions += projection.get_forward().num_edges();

    state.counters["num_transitions"] = benchmark::Counter(num_transitions);

    const auto instrumentation = p::get_projection_generator_instrumentation();
    state.counters["condition_join_calls"] = benchmark::Counter(instrumentation.condition_join_calls);
    state.counters["selected_static_literals"] = benchmark::Counter(instrumentation.selected_static_literals);
    state.counters["selected_fluent_literals"] = benchmark::Counter(instrumentation.selected_fluent_literals);
    state.counters["selected_zero_candidate_literals"] = benchmark::Counter(instrumentation.selected_zero_candidate_literals);
    state.counters["static_candidate_atoms_tried"] = benchmark::Counter(instrumentation.static_candidate_atoms_tried);
    state.counters["fluent_candidate_atoms_tried"] = benchmark::Counter(instrumentation.fluent_candidate_atoms_tried);
    state.counters["fluent_visible_branches"] = benchmark::Counter(instrumentation.fluent_visible_branches);
    state.counters["fluent_hidden_branches"] = benchmark::Counter(instrumentation.fluent_hidden_branches);
    state.counters["condition_binding_callbacks"] = benchmark::Counter(instrumentation.condition_binding_callbacks);
    state.counters["unifier_queries"] = benchmark::Counter(instrumentation.unifier_queries);
    state.counters["change_unification_calls"] = benchmark::Counter(instrumentation.change_unification_calls);
    state.counters["positive_effect_candidates_tried"] = benchmark::Counter(instrumentation.positive_effect_candidates_tried);
    state.counters["negative_effect_candidates_tried"] = benchmark::Counter(instrumentation.negative_effect_candidates_tried);
    state.counters["effect_condition_fire_attempts"] = benchmark::Counter(instrumentation.effect_condition_fire_attempts);
    state.counters["verified_binding_callbacks"] = benchmark::Counter(instrumentation.verified_binding_callbacks);
    state.counters["patterns_processed"] = benchmark::Counter(instrumentation.patterns_processed);
    state.counters["abstract_states_created"] = benchmark::Counter(instrumentation.abstract_states_created);
    state.counters["action_contexts_created"] = benchmark::Counter(instrumentation.action_contexts_created);
    state.counters["state_pair_checks"] = benchmark::Counter(instrumentation.state_pair_checks);
    state.counters["self_state_pairs_skipped"] = benchmark::Counter(instrumentation.self_state_pairs_skipped);
    state.counters["action_pair_checks"] = benchmark::Counter(instrumentation.action_pair_checks);
    state.counters["project_task_ns"] = benchmark::Counter(instrumentation.project_task_ns);
    state.counters["project_fdr_context_ns"] = benchmark::Counter(instrumentation.project_fdr_context_ns);
    state.counters["project_formalism_task_ns"] = benchmark::Counter(instrumentation.project_formalism_task_ns);
    state.counters["project_lifted_task_create_ns"] = benchmark::Counter(instrumentation.project_lifted_task_create_ns);
    state.counters["abstract_states_ns"] = benchmark::Counter(instrumentation.abstract_states_ns);
    state.counters["projection_context_ns"] = benchmark::Counter(instrumentation.projection_context_ns);
    state.counters["abstract_state_infos_ns"] = benchmark::Counter(instrumentation.abstract_state_infos_ns);
    state.counters["action_contexts_ns"] = benchmark::Counter(instrumentation.action_contexts_ns);
    state.counters["transition_loop_ns"] = benchmark::Counter(instrumentation.transition_loop_ns);
    state.counters["transition_generation_ns"] = benchmark::Counter(instrumentation.transition_generation_ns);
    state.counters["projection_assembly_ns"] = benchmark::Counter(instrumentation.projection_assembly_ns);
    state.counters["total_projection_ns"] = benchmark::Counter(instrumentation.total_projection_ns);

    auto heuristic = p::CanonicalHeuristic<p::LiftedTag>::create(projections);
    auto state_repository = p::StateRepository<p::LiftedTag>::create(task, tyr::ExecutionContext::create(1));
    const auto initial_state = state_repository->get_initial_state();
    const auto initial_heuristic_estimate = heuristic->evaluate(initial_state);

    state.counters["initial_heuristic_estimate"] = benchmark::Counter(initial_heuristic_estimate);
}
}

int main(int argc, char** argv)
{
    benchmark::Initialize(&argc, argv);

    for (const auto& benchmark_case : load_cases())
    {
        benchmark::RegisterBenchmark((benchmark_case.name + "/projection_generator").c_str(),
                                     [benchmark_case](benchmark::State& state) { benchmark_projection_generator(state, benchmark_case); });
    }

    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();

    return 0;
}
