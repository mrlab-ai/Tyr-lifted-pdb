"""
Prototype of lifted-PDBs using maximum over binary goal fact projections.

Example usage (run from the repository root):

    python3 python/prototypes/lifted_pdbs.py \
        -d data/gripper/domain.pddl \
        -p data/gripper/p-2-0.pddl

Author: Dominik Drexler (dominik.drexler@liu.se)
"""

import argparse

from pathlib import Path
import sys
import time

from lifted_ipdb import LiftedIPDBPatternGenerator
from pytyr.common import (
    ExecutionContext
)

from pytyr.formalism.planning import (
    ParserOptions, 
    Parser, 
)

from pytyr.planning import (
    Pattern,
)

from pytyr.planning.lifted import (
    Task, 
    SuccessorGenerator, 
    PatternGenerator,
    GoalPatternGenerator,
    ProjectionGenerator,
    ProjectionAbstractionHeuristic,
    CanonicalHeuristic,
    MaxHeuristic,
    BlindHeuristic,
    ProjectionAbstractionList,
    PruningStrategy, 
    TaskGoalStrategy,
)

from pytyr.planning.lifted.astar_eager import (
    Options, 
    DefaultEventHandler, 
    find_solution
)


class CustomPatternGenerator(PatternGenerator):
    """ Custom pattern generator
    """
    def generate(self) -> list[Pattern]: 
        pass

    
def main():
    global_start = time.perf_counter_ns()
    
    arg_parser = argparse.ArgumentParser(description="A* Eager Search.")
    arg_parser.add_argument("-d", "--domain-filepath", type=Path, required=True, help="Path to a PDDL domain file.")
    arg_parser.add_argument("-p", "--task-filepath", type=Path, required=True, help="Path to PDDL task file.")
    #arg_parser.add_argument("--max-pattern-size", type=int, default=2, help="Maximum size of generated patterns.")
    #arg_parser.add_argument("--max-pattern-count", type=int, default=10, help="Maximum number of generated patterns.")
    args = arg_parser.parse_args()

    domain_filepath : Path = args.domain_filepath
    task_filepath : Path = args.task_filepath

    parser_options = ParserOptions()
    parser = Parser(domain_filepath, parser_options)
    lifted_task = Task(parser.parse_task(task_filepath, parser_options))
    execution_context = ExecutionContext(1)
    successor_generator = SuccessorGenerator(lifted_task, execution_context)

    # BELOW: A hack to filter out projections whose PDBs report the initial state as a dead-end. 
    initial_node = successor_generator.get_initial_node()
    initial_state = initial_node.get_state()

    heuristic_start = time.perf_counter_ns()

    heuristic = BlindHeuristic()

    heuristic_end = time.perf_counter_ns()
    heuristic_time_ns = heuristic_end - heuristic_start
    heuristic_time_ms = heuristic_time_ns / 1_000_000

    print(f"[HEURISTIC] Heuristic computation time {heuristic_time_ms:.3f} ms ({heuristic_time_ns} ns)", flush=True)

    print(f"[HEURISTIC] Start node h-value: {heuristic.evaluate(initial_state)}", flush=True)

    options = Options()                               # Lifted search is parallelized but only useful on large tasks.
    options.event_handler = DefaultEventHandler(0)         # Collects and prints statistics. If verbosity >= 2, then also prints labeled nodes.
    options.goal_strategy = TaskGoalStrategy(lifted_task)  # Terminates the search when reaching a state that satisfies the task's goal.
    options.pruning_strategy = PruningStrategy()           # Never prunes

    search_start_time = time.perf_counter_ns()
    search_start_ns = search_start_time - global_start
    search_start_ms = search_start_ns / 1_000_000

    print(f"[SEARCH] Search start time {search_start_ms:.3f} ms ({search_start_ns} ns)", flush=True)

    search_result = find_solution(lifted_task, successor_generator, heuristic, options)
 
    print("Search status:", search_result.status, flush=True)

    plan = search_result.plan

    if plan is not None:
        print(f"Found plan with length {plan.get_length()} and cost {plan.get_cost()}", flush=True)
        print(plan)
    else:
        print("No solution was found.", flush=True)

    global_end = time.perf_counter_ns()

    print(f"Total time: {global_end - global_start} ns", flush=True)

if __name__ == "__main__":
    main()
