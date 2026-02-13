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

#include <argparse/argparse.hpp>
#include <chrono>
#include <fstream>
#include <queue>
#include <tyr/tyr.hpp>

using namespace tyr;

int main(int argc, char** argv)
{
    auto program = argparse::ArgumentParser("AStar eager search.");
    program.add_argument("-D", "--domain-filepath").required().help("The path to the PDDL domain file.");
    program.add_argument("-P", "--problem-filepath").required().help("The path to the PDDL problem file.");
    program.add_argument("-O", "--plan-filepath").default_value(std::string("plan.out")).help("The path to the output plan file.");
    program.add_argument("-N", "--num-worker-threads").default_value(size_t(1)).scan<'u', size_t>().help("The number of worker threads.");
    program.add_argument("-V", "--verbosity")
        .default_value(size_t(0))
        .scan<'u', size_t>()
        .help("The verbosity level. Defaults to minimal amount of debug output.");

    try
    {
        program.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err)
    {
        std::cerr << err.what() << "\n";
        std::cerr << program;
        std::exit(1);
    }

    auto total_time = std::chrono::nanoseconds { 0 };
    {
        auto stop_watch = StopwatchScope(total_time);

        auto domain_filepath = program.get<std::string>("--domain-filepath");
        auto problem_filepath = program.get<std::string>("--problem-filepath");
        auto plan_filepath = program.get<std::string>("--plan-filepath");
        oneapi::tbb::global_control control(oneapi::tbb::global_control::max_allowed_parallelism, program.get<std::size_t>("--num-worker-threads"));
        auto verbosity = program.get<size_t>("--verbosity");

        auto parser_options = loki::ParserOptions();
        // parser_options.strict = true;
        auto parser = planning::Parser(domain_filepath, parser_options);
        auto domain = parser.get_domain();

        auto lifted_task = parser.parse_task(problem_filepath);

        if (verbosity > 0)
            std::cout << *domain << std::endl;

        if (verbosity > 0)
            std::cout << *lifted_task << std::endl;

        auto successor_generator = planning::SuccessorGenerator<planning::LiftedTask>(lifted_task);

        auto options = planning::astar_eager::Options<planning::LiftedTask>();
        options.start_node = successor_generator.get_initial_node();
        options.event_handler = planning::astar_eager::DefaultEventHandler<planning::LiftedTask>::create(verbosity);

        auto patterns = planning::PatternGenerator<planning::LiftedTask>(*lifted_task).generate();
        print(std::cout, patterns);
        std::cout << std::endl;

        // auto projections = planning::ProjectionGenerator<planning::LiftedTask>(*lifted_task, patterns);
        // auto canonical_heuristic = planning::CanonicalHeuristic::create(*lifted_task, projections);
        // auto scp_heuristic = planning::SaturatedCostPartitioningHeuristic::create(*lifted_task, projections);

        auto ff_heuristic = planning::FFHeuristic<planning::LiftedTask>::create(lifted_task);
        ff_heuristic->set_goal(lifted_task->get_task().get_goal());

        auto result = planning::astar_eager::find_solution(*lifted_task, successor_generator, *ff_heuristic, options);

        if (result.status == planning::SearchStatus::SOLVED)
        {
            std::ofstream plan_file;
            plan_file.open(plan_filepath);
            if (!plan_file.is_open())
            {
                std::cerr << "Error opening file!" << std::endl;
                return 1;
            }
            plan_file << result.plan.value();
            plan_file.close();
        }
    }

    std::cout << "[Total] Peak memory usage: " << get_peak_memory_usage_in_bytes() << " bytes" << std::endl;
    std::cout << "[Total] Total time: " << to_ms(total_time) << " ms (" << to_ns(total_time) << " ns)" << std::endl;

    return 0;
}