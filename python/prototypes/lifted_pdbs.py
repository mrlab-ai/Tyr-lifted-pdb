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

from pattern_gen import (
    LiftedPatternGenerator,
    LiftedInterestingPatternGenerator,
)
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
    ProjectionOptions,
    FluentLiteralOrder,
    SrcAtomsIndex,
    NegativeLiteralPushdown,
    InequalityPropagation,
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
    arg_parser.add_argument("--max-pattern-size", type=int, default=2, help="Maximum size of generated patterns.")
    arg_parser.add_argument("--max-pattern-count", type=int, default=10, help="Maximum number of generated patterns.")
    arg_parser.add_argument("--projection-fluent-literal-order",
                            choices=["declaration", "selectivity"], default="selectivity",
                            help="Phase 4a: ordering of positive fluent precondition literals (default: selectivity).")
    arg_parser.add_argument("--projection-src-atoms-index",
                            choices=["off", "on"], default="on",
                            help="Phase 4b: per-src-state predicate index over visible fluent atoms (default: on).")
    arg_parser.add_argument("--projection-negative-literal-pushdown",
                            choices=["off", "on"], default="on",
                            help="Phase 4c: push negative fluent literal checks down to the earliest "
                                 "checkpoint where their parameters are bound (default: on).")
    arg_parser.add_argument("--projection-inequality-propagation",
                            choices=["off", "on"], default="on",
                            help="Phase 4d: pull `(not (= ?x ?y))` constraints out of the static "
                                 "join and check them via direct object-identity comparison at the "
                                 "earliest checkpoint where both terms are bound (default: on).")
    arg_parser.add_argument("--projection-dedup-stats",
                            action="store_true", default=False,
                            help="Diagnostic: emit [DEDUP-STATS] lines per pattern and per action "
                                 "reporting how many emitted transitions collapse to distinct "
                                 "(src, dst[, action]) edges. Used to size the potential gain of a "
                                 "projected-enumeration redesign (Lauer-style regression substrate).")
    arg_parser.add_argument("--pattern-gen-fallback-bound",
                            choices=["off", "on"], default="on",
                            help="Phase 6.5: bound the schema-co-occurrence fallback in pattern "
                                 "generation. Only consulted when --pattern-gen-static-csp=off. "
                                 "When on, co-predicates that share no schema variable with the "
                                 "pattern predicate contribute only their initial-state atoms, "
                                 "or up to 32 reachable atoms if effect-only. When off, the "
                                 "fallback enumerates every reachable atom (pre-2026-05-26).")
    arg_parser.add_argument("--pattern-gen-static-csp",
                            choices=["off", "on"], default="on",
                            help="Phase 6.6: honour static preconditions in the causal-edge test "
                                 "during pattern generation. When on (default), every candidate "
                                 "neighbour is filtered by a static-CSP feasibility check — only "
                                 "candidates for which some action grounding exists that maps the "
                                 "effect atom to the goal, the precondition atom to the candidate, "
                                 "and satisfies all the action's static preconditions are kept. "
                                 "This subsumes --pattern-gen-fallback-bound and is the principled "
                                 "alignment with Scorpion's interesting-pattern filter for sys2. "
                                 "When off, the old var-linked / fallback split is used.")
    arg_parser.add_argument("--pattern-gen-reachability",
                            choices=["off", "on"], default="on",
                            help="Phase 6.7: delete-relaxation reachability filter (only with "
                                 "--pattern-gen-static-csp=on). When on (default), a candidate is "
                                 "kept only if it AND every fluent precondition of the action are "
                                 "reachable (in the delete-relaxation reachable atom set R+). This "
                                 "reproduces Scorpion's operator-applicability pruning (e.g. drops "
                                 "logistics' airplane patterns whose precondition at(airplane, "
                                 "non-airport) is unreachable). R+ is computed natively via Tyr's "
                                 "RelaxedReachability — same monotone-fixpoint engine successor "
                                 "generation uses, sub-second on HTG-large.")
    arg_parser.add_argument("--pattern-gen-scorpion-match",
                            choices=["off", "on"], default="off",
                            help="Phase 6.9: enable SAS+-style no-op simplification of the "
                                 "action-edge CSP. When on, synthetic inequalities reject "
                                 "action edges whose effect atom would collapse to one of the "
                                 "action's other positive fluent preconditions under every "
                                 "feasible binding. Mirrors Scorpion's SAS+ simplification (an "
                                 "effect that already matches an existing precondition is "
                                 "dropped, removing its causal edge). Fixes the +3 over-count "
                                 "Tyr-SGA exhibits on organic-synthesis-alkene p12/p13. The 5 "
                                 "UNDER tasks (p2/p6/p7/p8/p18) where Scorpion-SGA has more "
                                 "patterns are NOT addressed; those extras are over unreachable "
                                 "atoms and so are heuristically inert.")
    arg_parser.add_argument("--pattern-gen-interesting",
                            choices=["off", "on"], default="off",
                            help="Phase 6.10: switch pattern generation from SGA "
                                 "(`LiftedPatternGenerator`, default) to interesting "
                                 "(`LiftedInterestingPatternGenerator`). Interesting extends SGA "
                                 "with a disjoint-union step over (eff, eff) co-effect arcs, "
                                 "matching Scorpion's `pattern_type=interesting` at sys2. "
                                 "Implemented for paper-experiment ablation only — empirically "
                                 "SGA dominates on HTG, so this flag is intended to *demonstrate* "
                                 "that interesting does not help, not as a production setting.")
    args = arg_parser.parse_args()

    projection_options = ProjectionOptions()
    projection_options.fluent_literal_order = (
        FluentLiteralOrder.Selectivity
        if args.projection_fluent_literal_order == "selectivity"
        else FluentLiteralOrder.Declaration
    )
    projection_options.src_atoms_index = (
        SrcAtomsIndex.On if args.projection_src_atoms_index == "on" else SrcAtomsIndex.Off
    )
    projection_options.negative_literal_pushdown = (
        NegativeLiteralPushdown.On if args.projection_negative_literal_pushdown == "on" else NegativeLiteralPushdown.Off
    )
    projection_options.inequality_propagation = (
        InequalityPropagation.On if args.projection_inequality_propagation == "on" else InequalityPropagation.Off
    )
    projection_options.collect_dedup_stats = bool(args.projection_dedup_stats)
    print(f"[PROJECT] fluent_literal_order={args.projection_fluent_literal_order} "
          f"src_atoms_index={args.projection_src_atoms_index} "
          f"negative_literal_pushdown={args.projection_negative_literal_pushdown} "
          f"inequality_propagation={args.projection_inequality_propagation} "
          f"dedup_stats={'on' if args.projection_dedup_stats else 'off'}", flush=True)
    print(f"[PATTERN] fallback_bound={args.pattern_gen_fallback_bound} "
          f"static_csp={args.pattern_gen_static_csp} "
          f"reachability={args.pattern_gen_reachability} "
          f"scorpion_match={args.pattern_gen_scorpion_match} "
          f"interesting={args.pattern_gen_interesting}", flush=True)
    pattern_gen_bounded_fallback = (args.pattern_gen_fallback_bound == "on")
    pattern_gen_static_csp = (args.pattern_gen_static_csp == "on")
    pattern_gen_reachability = (args.pattern_gen_reachability == "on")
    pattern_gen_scorpion_match = (args.pattern_gen_scorpion_match == "on")
    pattern_gen_interesting = (args.pattern_gen_interesting == "on")

    domain_filepath : Path = args.domain_filepath
    task_filepath : Path = args.task_filepath

    parser_options = ParserOptions()
    parser = Parser(domain_filepath, parser_options)
    lifted_task = Task(parser.parse_task(task_filepath, parser_options))
    execution_context = ExecutionContext(1)
    successor_generator = SuccessorGenerator(lifted_task, execution_context)

    # Use the lifted iPDB-style pattern generator with CLI-controlled limits.
    print("[PATTERN] Pattern generation started", flush=True)

    pattern_start = time.perf_counter_ns()

    pattern_gen_cls = (LiftedInterestingPatternGenerator
                       if pattern_gen_interesting
                       else LiftedPatternGenerator)
    patterns = pattern_gen_cls(
        lifted_task,
        bounded_fallback=pattern_gen_bounded_fallback,
        static_csp=pattern_gen_static_csp,
        reachability=pattern_gen_reachability,
        scorpion_match=pattern_gen_scorpion_match,
    ).generate(
        args.max_pattern_size,
        args.max_pattern_count,
    )

    pattern_end = time.perf_counter_ns()
    pattern_time_ns = pattern_end - pattern_start
    pattern_time_ms = pattern_time_ns / 1_000_000

    print(f"[PATTERN] Pattern generation time {pattern_time_ms:.3f} ms ({pattern_time_ns} ns)", flush=True)
    print(f"[PATTERN] Generated {len(patterns)} patterns.", flush=True)

    print("[PROJECT] Projection computation started")
    proj_start = time.perf_counter_ns()
    
    projections = ProjectionGenerator(lifted_task, patterns, projection_options).generate()

    proj_end = time.perf_counter_ns()
    proj_time_ns = proj_end - proj_start
    proj_time_ms = proj_time_ns / 1_000_000

    print(f"[PROJECT] Projections computation time {proj_time_ms:.3f} ms ({proj_time_ns} ns)", flush=True)

    # BELOW: A hack to filter out projections whose PDBs report the initial state as a dead-end. 
    initial_node = successor_generator.get_initial_node()
    initial_state = initial_node.get_state()
    """
    filtered_projections = ProjectionAbstractionList()
    for proj in projections:
        ph = ProjectionAbstractionHeuristic(proj)
        h0 = ph.evaluate(initial_state)
       if h0 == float("inf"):
            print("[WARN] Dropping projection whose PDB reports the initial state as a dead-end (h = inf).")
            continue
        filtered_projections.append(proj)

    if len(filtered_projections) == 0:
        print("[WARN] All projections reported the initial state as a dead-end. Falling back to BlindHeuristic.")
        heuristic = BlindHeuristic()
    else:
    """
    heuristic_start = time.perf_counter_ns()

    heuristic = CanonicalHeuristic(projections)

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

    # Resident-set high-water-mark (KB on Linux). Captured AFTER all
    # processing so it includes pattern gen + projection + search.
    import resource
    peak_kb = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    print(f"[MEMORY] Peak memory usage {peak_kb} KB", flush=True)

if __name__ == "__main__":
    main()
