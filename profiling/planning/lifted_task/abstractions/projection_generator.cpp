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
        projections = p::ProjectionGenerator<p::LiftedTag>(task, patterns).generate();

        benchmark::DoNotOptimize(projections);
    }

    auto heuristic = p::CanonicalHeuristic<p::LiftedTag>::create(projections);
    auto state_repository = p::StateRepository<p::LiftedTag>::create(task, tyr::ExecutionContext::create(1));
    const auto initial_state = state_repository->get_initial_state();
    const auto initial_heuristic_estimate = heuristic->evaluate(initial_state);

    state.counters["initial_heuristic_estimate"] = benchmark::Counter(static_cast<double>(initial_heuristic_estimate));
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
