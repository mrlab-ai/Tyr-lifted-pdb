"""
Compare iPDB vs systematic (goal) patterns of size 1.

Metrics reported per configuration and problem instance:
  - h(s0): heuristic value of the initial state
  - last_f_expansions: node expansions in the last completed f-layer

Example usage (run from the repository root)::

    python3 python/prototypes/compare_ipdb_systematic.py
"""

import sys
from pathlib import Path

from pytyr.common import ExecutionContext
from pytyr.formalism.planning import Parser, ParserOptions
from pytyr.planning.lifted import (
    Task,
    SuccessorGenerator,
    GoalPatternGenerator,
    ProjectionGenerator,
    CanonicalHeuristic,
    PruningStrategy,
    TaskGoalStrategy,
)
from pytyr.planning.lifted.astar_eager import EventHandler, Options, find_solution

from lifted_ipdb import LiftedIPDBPatternGenerator


# ---------------------------------------------------------------------------
# Event handler that tracks expansions per f-layer
# ---------------------------------------------------------------------------

class LastFLayerTracker(EventHandler):
    """Counts node expansions; records the count for each completed f-layer."""

    # All EventHandler methods are pure virtual in C++ and must be overridden.

    def on_expand_node(self, node) -> None:
        self._current += 1

    def on_expand_goal_node(self, node) -> None:
        pass

    def on_generate_node(self, labeled_succ_node) -> None:
        pass

    def on_generate_node_relaxed(self, labeled_succ_node) -> None:
        pass

    def on_generate_node_not_relaxed(self, labeled_succ_node) -> None:
        pass

    def on_close_node(self, node) -> None:
        pass

    def on_prune_node(self, node) -> None:
        pass

    def on_start_search(self, node, f_value: float) -> None:
        pass

    def on_finish_f_layer(self, f_value: float) -> None:
        self.last_f_expansions = self._current
        self.last_f_value = f_value
        self._current = 0

    def on_end_search(self) -> None:
        pass

    def on_solved(self, plan) -> None:
        pass

    def on_unsolvable(self) -> None:
        pass

    def on_exhausted(self) -> None:
        pass


# ---------------------------------------------------------------------------
# Helper: run A* and return (h0, last_f_expansions, status)
# ---------------------------------------------------------------------------

def run_astar(task: Task, heuristic, succ_gen: SuccessorGenerator):
    initial_state = succ_gen.get_initial_node().get_state()
    h0 = heuristic.evaluate(initial_state)

    tracker = LastFLayerTracker()
    tracker._current = 0
    tracker.last_f_expansions = 0
    tracker.last_f_value = None
    opts = Options()
    opts.event_handler = tracker
    opts.goal_strategy = TaskGoalStrategy(task)
    opts.pruning_strategy = PruningStrategy()

    result = find_solution(task, succ_gen, heuristic, opts)
    return h0, tracker.last_f_expansions, result.status


# ---------------------------------------------------------------------------
# Helper: build heuristic from a pattern list
# ---------------------------------------------------------------------------

def make_canonical(task: Task, patterns) -> CanonicalHeuristic:
    projections = ProjectionGenerator(task, patterns).generate()
    return CanonicalHeuristic(projections)


# ---------------------------------------------------------------------------
# Test instances
# ---------------------------------------------------------------------------

ROOT = Path(__file__).resolve().parents[2]  # repo root

INSTANCES = [
    {
        "name": "genome-edit-distance d-1-2",
        "domain": ROOT / "data/genome-edit-distance/domain.pddl",
        "problem": ROOT / "data/genome-edit-distance/d-1-2.pddl",
    },
    {
        "name": "childsnack contentam1-cham3-p0",
        "domain": ROOT / "data/childsnack-contents/domain.pddl",
        "problem": ROOT / "data/childsnack-contents/contentam1-cham3-p0.pddl",
    },
]


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def run_instance(inst: dict) -> None:
    name = inst["name"]
    print(f"\n{'='*60}")
    print(f"Instance: {name}")
    print(f"{'='*60}")

    parser_opts = ParserOptions()
    parser = Parser(inst["domain"], parser_opts)
    task = Task(parser.parse_task(inst["problem"], parser_opts))
    exec_ctx = ExecutionContext(1)
    succ_gen = SuccessorGenerator(task, exec_ctx)

    # ── Systematic (goal) patterns of size 1 ──
    print("\n[Systematic size-1]")
    sys_patterns = GoalPatternGenerator(task).generate()
    print(f"  Patterns: {len(sys_patterns)}")
    sys_h = make_canonical(task, sys_patterns)
    sys_h0, sys_last, sys_status = run_astar(task, sys_h, succ_gen)
    print(f"  h(s0)              = {sys_h0}")
    print(f"  last-f expansions  = {sys_last}")
    print(f"  search status      = {sys_status}")

    # ── iPDB patterns (size 2) ──
    print("\n[iPDB size-2]")
    ipdb_patterns = LiftedIPDBPatternGenerator(task).generate(
        max_pattern_size=2, max_pattern_count=10
    )
    print(f"  Patterns: {len(ipdb_patterns)}")
    ipdb_h = make_canonical(task, ipdb_patterns)
    ipdb_h0, ipdb_last, ipdb_status = run_astar(task, ipdb_h, succ_gen)
    print(f"  h(s0)              = {ipdb_h0}")
    print(f"  last-f expansions  = {ipdb_last}")
    print(f"  search status      = {ipdb_status}")

    # ── Summary ──
    print("\n[Summary]")
    print(f"  h(s0):             systematic={sys_h0}  ipdb={ipdb_h0}  "
          f"delta={ipdb_h0 - sys_h0:+.4f}")
    print(f"  last-f expansions: systematic={sys_last}  ipdb={ipdb_last}  "
          f"ratio={'N/A' if sys_last == 0 else f'{ipdb_last/sys_last:.3f}'}")


def main():
    for inst in INSTANCES:
        try:
            run_instance(inst)
        except Exception as exc:
            print(f"\n[ERROR] {inst['name']}: {exc}", file=sys.stderr)

    print("\nDone.", flush=True)


if __name__ == "__main__":
    main()
