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

#include <benchmark/benchmark.h>
#include <boost/json.hpp>
#include <random>
#include <tyr/tyr.hpp>

namespace json = boost::json;

namespace tyr::benchmarks
{
namespace
{
fs::path absolute(const std::string& prefix, const std::string& subdir) { return fs::path(prefix) / subdir; }

struct BenchmarkInstance
{
    std::string name;
    std::string domain_filepath;
    std::string problem_filepath;
};

std::string read_file(const fs::path& filepath)
{
    std::ifstream in(filepath);
    if (!in)
        throw std::runtime_error("Could not open benchmark config: " + filepath.string());

    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::vector<BenchmarkInstance> read_benchmark_config(const fs::path& filepath)
{
    const auto content = read_file(filepath);
    const auto root = json::parse(content).as_object();

    std::vector<BenchmarkInstance> instances;

    for (const auto& [domain_name, domain_value] : root)
    {
        const auto& domain_obj = domain_value.as_object();

        const auto domain_filepath = absolute(std::string(DATA_DIR), std::string(domain_obj.at("domain").as_string()));

        const auto& tasks = domain_obj.at("tasks").as_array();

        for (const auto& task_value : tasks)
        {
            const auto problem_filepath = absolute(std::string(DATA_DIR), std::string(task_value.as_string()));

            const auto task_stem = fs::path(std::string(task_value.as_string())).stem().string();

            instances.push_back(BenchmarkInstance {
                .name = std::string(domain_name) + "/" + task_stem,
                .domain_filepath = domain_filepath.string(),
                .problem_filepath = problem_filepath.string(),
            });
        }
    }

    return instances;
}

void BM_ProjectionGenerator(benchmark::State& state, std::string domain_filepath, std::string problem_filepath)
{
    for (auto _ : state)
    {
        auto parser_options = loki::ParserOptions();
        auto parser = formalism::planning::Parser(domain_filepath, parser_options);

        auto domain = parser.get_domain();
        auto lifted_task = planning::LiftedTask::create(parser.parse_task(problem_filepath));

        auto patterns = planning::GoalPatternGenerator<planning::LiftedTag>(lifted_task).generate();

        auto projections = planning::ProjectionGenerator<planning::LiftedTag>(lifted_task, patterns).generate();

        benchmark::DoNotOptimize(domain);
        benchmark::DoNotOptimize(lifted_task);
        benchmark::DoNotOptimize(patterns);
        benchmark::DoNotOptimize(projections);
    }
}

void register_projection_generator_benchmarks()
{
    const auto config_path = absolute(std::string(PROFILING_DIR), "planning/lifted_task/abstractions/projection_generator.json");
    const auto instances = read_benchmark_config(config_path);

    for (const auto& instance : instances)
    {
        benchmark::RegisterBenchmark(("BM_ProjectionGenerator/" + instance.name).c_str(),
                                     BM_ProjectionGenerator,
                                     instance.domain_filepath,
                                     instance.problem_filepath);
    }
}
}

}

int main(int argc, char** argv)
{
    tyr::benchmarks::register_projection_generator_benchmarks();

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();

    return 0;
}
