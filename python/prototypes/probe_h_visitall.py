"""Probe Tyr's PDB heuristic on visitall-FAR-g1/p0.

The 04-06-04 HTG run showed Tyr OOMing during A* on visitall-FAR while
Scorpion solves the same task in 214 expansions. Both report h0=1.
Hypothesis: Tyr's h(s) stays ~1 across the state space (degenerate
heuristic), whereas Scorpion's h(s) grows as the search moves away from
the start. This script BFS-walks the state space from the initial state
and records the distribution of h(s) at each depth d, so we can see
whether the heuristic is informative on non-initial states.

Run:
    python python/prototypes/probe_h_visitall.py
"""

import sys
import statistics
from pathlib import Path

from pytyr.formalism.planning import Parser, ParserOptions
from pytyr.common import ExecutionContext
from pytyr.planning.lifted import (
    Task, SuccessorGenerator,
    ProjectionGenerator, ProjectionOptions,
    LiftedSystematicPatternGenerator, LiftedSystematicPatternGeneratorOptions,
    CanonicalHeuristic,
    FluentLiteralOrder, SrcAtomsIndex, NegativeLiteralPushdown, InequalityPropagation,
)

DOMAIN = "/proj/naiss2026-4-697/users/x_miksk/Tyr-lifted-pdb/data/htg-domains/flat/visitall-multidimensional-3-dim-visitall-FAR-g1/domain.pddl"
PROBLEM = "/proj/naiss2026-4-697/users/x_miksk/Tyr-lifted-pdb/data/htg-domains/flat/visitall-multidimensional-3-dim-visitall-FAR-g1/p0.pddl"


def main():
    parser_options = ParserOptions()
    parser = Parser(DOMAIN, parser_options)
    lifted_task = Task(parser.parse_task(PROBLEM, parser_options))

    execution_context = ExecutionContext(1)
    successor_generator = SuccessorGenerator(lifted_task, execution_context)
    successor_generator.set_use_unit_cost(True)

    proj_opts = ProjectionOptions()
    proj_opts.fluent_literal_order = FluentLiteralOrder.Selectivity
    proj_opts.src_atoms_index = SrcAtomsIndex.On
    proj_opts.negative_literal_pushdown = NegativeLiteralPushdown.On
    proj_opts.inequality_propagation = InequalityPropagation.On
    proj_opts.reachability_filter = True

    pg_opts = LiftedSystematicPatternGeneratorOptions()
    pg_opts.bounded_fallback = False
    pg_opts.static_csp = True
    pg_opts.reachability = True
    pg_opts.scorpion_match = False
    pg_opts.interesting = True
    pg_opts.max_pattern_size = 2
    pg_opts.max_pattern_count = 10000

    patterns = LiftedSystematicPatternGenerator(lifted_task, pg_opts).generate()
    print(f"[probe] {len(patterns)} patterns generated")
    for i, p in enumerate(patterns):
        print(f"[probe]   pattern {i}: {p}")

    projections = ProjectionGenerator(lifted_task, patterns, proj_opts).generate()
    heuristic = CanonicalHeuristic(projections)

    state_repository = successor_generator.get_state_repository()
    initial_node = successor_generator.get_initial_node()
    initial_state = initial_node.get_state()
    initial_state_index = initial_state.get_index()

    h0 = heuristic.evaluate(initial_state)
    print(f"[probe] h(initial) = {h0}")

    # BFS to depth K, sampling at most CAP states per layer.
    import random
    random.seed(0)
    K = 16
    CAP = 500
    frontier = [initial_state_index]
    seen = {initial_state_index}
    depth = 0
    while depth < K and frontier:
        hs = []
        for sidx in frontier:
            s = state_repository.get_registered_state(sidx)
            h = heuristic.evaluate(s)
            hs.append(h)
        uniq = sorted(set(hs))
        print(f"[probe] depth={depth:2d}  n_states={len(frontier):>6}  "
              f"h: min={min(hs)} max={max(hs)} mean={statistics.mean(hs):.3f} "
              f"unique={uniq[:6]}{'...' if len(uniq)>6 else ''}")
        # Expand (sampled)
        sample = frontier if len(frontier) <= CAP else random.sample(frontier, CAP)
        next_frontier = []
        for sidx in sample:
            node = successor_generator.get_node(sidx)
            for lsn in successor_generator.get_labeled_successor_nodes(node):
                ss = lsn.node.get_state()
                ssi = ss.get_index()
                if ssi not in seen:
                    seen.add(ssi)
                    next_frontier.append(ssi)
        frontier = next_frontier
        depth += 1


if __name__ == "__main__":
    main()
