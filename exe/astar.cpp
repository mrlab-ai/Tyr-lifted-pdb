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
#include <queue>
#include <tyr/tyr.hpp>

using namespace tyr;

enum SearchNodeStatus : uint8_t
{
    GOAL = 0,
    DEAD_END = 1,
    CLOSED = 2,
    OPEN = 3,
    NEW = 4,
};

struct SearchNode
{
    tyr::float_t g_value;
    planning::StateIndex parent;
    SearchNodeStatus status;
};

struct QueueEntry
{
    using KeyType = std::pair<tyr::float_t, SearchNodeStatus>;
    using ItemType = std::pair<tyr::float_t, planning::StateIndex>;

    tyr::float_t f_value;
    planning::StateIndex state;
    SearchNodeStatus status;

    KeyType get_key() const { return std::make_pair(f_value, status); }
    ItemType get_item() const { return std::make_pair(f_value, state); }
};

struct EntryComparator
{
    bool operator()(const QueueEntry& l, const QueueEntry& r) { return l.get_key() > r.get_key(); }
};

using Openlist = std::priority_queue<QueueEntry, std::vector<QueueEntry>, EntryComparator>;

inline auto& get_or_create_search_node(planning::StateIndex state, SegmentedVector<SearchNode>& search_nodes)
{
    static auto default_node = SearchNode { std::numeric_limits<tyr::float_t>::infinity(), planning::StateIndex(), SearchNodeStatus::NEW };

    while (uint_t(state) >= search_nodes.size())
    {
        search_nodes.push_back(default_node);
    }
    return search_nodes[uint_t(state)];
}

int main(int argc, char** argv)
{
    auto program = argparse::ArgumentParser("AStar search.");
    program.add_argument("-D", "--domain-filepath").required().help("The path to the PDDL domain file.");
    program.add_argument("-P", "--problem-filepath").required().help("The path to the PDDL problem file.");
    program.add_argument("-N", "--num-worker-threads").default_value(size_t(1)).scan<'u', size_t>().help("The number of worker threads.");

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

    auto domain_filepath = program.get<std::string>("--domain-filepath");
    auto problem_filepath = program.get<std::string>("--problem-filepath");
    oneapi::tbb::global_control control(oneapi::tbb::global_control::max_allowed_parallelism, program.get<std::size_t>("--num-worker-threads"));

    auto parser = planning::Parser(domain_filepath);
    auto domain = parser.get_domain();

    auto lifted_task = parser.parse_task(problem_filepath);

    auto initial_node = lifted_task->get_initial_node();

    auto queue = Openlist();

    auto search_nodes = SegmentedVector<SearchNode> {};

    const auto initial_f_value = initial_node.get_metric() + 0.;
    queue.emplace(initial_f_value, initial_node.get_state().get_index(), SearchNodeStatus::NEW);

    auto& initial_search_node = get_or_create_search_node(initial_node.get_state().get_index(), search_nodes);
    initial_search_node.g_value = initial_node.get_metric();

    while (!queue.empty())
    {
        auto entry = queue.top();
        queue.pop();

        const auto& search_node = get_or_create_search_node(entry.state, search_nodes);

        auto node = planning::Node<planning::LiftedTask>(lifted_task->get_state(entry.state), search_node.g_value);
    }

    return 0;
}