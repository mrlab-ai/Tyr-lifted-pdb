"""
Prototype of lifted-PDBs using maximum over binary goal fact projections.

Example usage (run from the repository root):

    python3 python/prototypes/lifted_pdbs.py \
        -d data/gripper/domain.pddl \
        -p data/gripper/test_problem.pddl

Author: Dominik Drexler (dominik.drexler@liu.se)
"""

import argparse

from pathlib import Path

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
    MaxHeuristic, 
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
    arg_parser = argparse.ArgumentParser(description="A* Eager Search.")
    arg_parser.add_argument("-d", "--domain-filepath", type=Path, required=True, help="Path to a PDDL domain file.")
    arg_parser.add_argument("-p", "--task-filepath", type=Path, required=True, help="Path to PDDL task file.")
    args = arg_parser.parse_args()

    domain_filepath : Path = args.domain_filepath
    task_filepath : Path = args.task_filepath

    parser_options = ParserOptions()
    parser = Parser(domain_filepath, parser_options)
    lifted_task = Task(parser.parse_task(task_filepath, parser_options))
    execution_context = ExecutionContext(1)
    successor_generator = SuccessorGenerator(lifted_task, execution_context)

    patterns = GoalPatternGenerator(lifted_task).generate()
    projections = ProjectionGenerator(lifted_task, patterns).generate()
    components = []
    for projection in projections:
        components.append(ProjectionAbstractionHeuristic(projection))
    heuristic = MaxHeuristic(components)

    options = Options()                               # Lifted search is parallelized but only useful on large tasks.
    options.event_handler = DefaultEventHandler(0)         # Collects and prints statistics. If verbosity >= 2, then also prints labeled nodes.
    options.goal_strategy = TaskGoalStrategy(lifted_task)  # Terminates the search when reaching a state that satisfies the task's goal.
    options.pruning_strategy = PruningStrategy()           # Never prunes

    search_result = find_solution(lifted_task, successor_generator, heuristic, options)
 
    print("Search status:", search_result.status)

    plan = search_result.plan

    if plan is not None:
        print(f"Found plan with length {plan.get_length()} and cost {plan.get_cost()}")
        print(plan)
    else:
        print("No solution was found.")


if __name__ == "__main__":
    main()