"""
Focused benchmark harness for autoresearch.

Measures ONLY the projection-build phase of the lifted-PDB pipeline — the
optimization target — using the *exact* same setup (flags, pattern generation,
ProjectionOptions) as python/prototypes/lifted_pdbs.py with the pinned
configuration. It deliberately skips CanonicalHeuristic construction and A*
search, which are not part of the metric and would make each repetition slow.

Output (parsed by autoresearch.sh / autoresearch.checks.sh):
    PROJ_MS=<float>      projection-build wall time in milliseconds
    NPROJ=<int>          number of projections produced
    NPAT=<int>           number of patterns produced
    SIG=<string>         correctness signature (only with --sig); identical
                         projections -> identical signature. Currently the
                         canonical-heuristic initial-state h-value plus the
                         sorted per-projection initial h-values.

The flag surface mirrors lifted_pdbs.py 1:1 so the pinned command maps directly.
"""

import argparse
import re
import sys
import tempfile
import atexit
import os
import time
from pathlib import Path

from pattern_gen import (
    LiftedPatternGenerator,
    LiftedInterestingPatternGenerator,
)
from pytyr.planning.lifted import (
    LiftedSystematicPatternGenerator,
    LiftedSystematicPatternGeneratorOptions,
)
from pytyr.common import ExecutionContext
from pytyr.formalism.planning import ParserOptions, Parser
from pytyr.planning.lifted import (
    Task,
    SuccessorGenerator,
    ProjectionGenerator,
    ProjectionOptions,
    FluentLiteralOrder,
    SrcAtomsIndex,
    NegativeLiteralPushdown,
    InequalityPropagation,
    ProjectionAbstractionHeuristic,
    CanonicalHeuristic,
)


def build_arg_parser():
    p = argparse.ArgumentParser(description="Projection-build micro-benchmark.")
    p.add_argument("-d", "--domain-filepath", type=Path, required=True)
    p.add_argument("-p", "--task-filepath", type=Path, required=True)
    p.add_argument("--max-pattern-size", type=int, default=2)
    p.add_argument("--max-pattern-count", type=int, default=10)
    p.add_argument("--projection-fluent-literal-order",
                   choices=["declaration", "selectivity"], default="selectivity")
    p.add_argument("--projection-src-atoms-index", choices=["off", "on"], default="on")
    p.add_argument("--projection-negative-literal-pushdown", choices=["off", "on"], default="on")
    p.add_argument("--projection-inequality-propagation", choices=["off", "on"], default="on")
    p.add_argument("--projection-reachability-filter", choices=["off", "on"], default="off")
    p.add_argument("--cost-type", choices=["original", "one"], default="original")
    p.add_argument("--pattern-gen-fallback-bound", choices=["off", "on"], default="on")
    p.add_argument("--pattern-gen-static-csp", choices=["off", "on"], default="on")
    p.add_argument("--pattern-gen-reachability", choices=["off", "on"], default="on")
    p.add_argument("--pattern-gen-scorpion-match", choices=["off", "on"], default="off")
    p.add_argument("--pattern-gen-interesting", choices=["off", "on"], default="off")
    p.add_argument("--pattern-gen-backend",
                   choices=["cpp-systematic", "py-systematic", "py-interesting", "py-ipdb"],
                   default="cpp-systematic")
    p.add_argument("--ipdb-samples", type=int, default=1000)
    p.add_argument("--ipdb-walk-length", type=int, default=10)
    # benchmark-only:
    p.add_argument("--sig", action="store_true",
                   help="also compute and print a correctness signature (slow).")
    return p


def maybe_inject_neg_preconds(filepath):
    text = filepath.read_text()
    m = re.search(r'\(:requirements\s+([^)]*)\)', text)
    if m is None:
        return filepath
    reqs = m.group(1)
    if (':negative-preconditions' in reqs or ':adl' in reqs or
            ':quantified-preconditions' in reqs):
        return filepath
    new_block = f'(:requirements {reqs.rstrip()} :negative-preconditions)'
    new_text = text[:m.start()] + new_block + text[m.end():]
    tmp = tempfile.NamedTemporaryFile(mode='w', suffix='.pddl', delete=False,
                                      prefix=f'_tyr_inject_{filepath.stem}_')
    tmp.write(new_text)
    tmp.close()
    atexit.register(lambda p=tmp.name: os.unlink(p) if os.path.exists(p) else None)
    return Path(tmp.name)


def make_projection_options(args):
    o = ProjectionOptions()
    o.fluent_literal_order = (FluentLiteralOrder.Selectivity
                              if args.projection_fluent_literal_order == "selectivity"
                              else FluentLiteralOrder.Declaration)
    o.src_atoms_index = (SrcAtomsIndex.On if args.projection_src_atoms_index == "on"
                         else SrcAtomsIndex.Off)
    o.negative_literal_pushdown = (NegativeLiteralPushdown.On
                                   if args.projection_negative_literal_pushdown == "on"
                                   else NegativeLiteralPushdown.Off)
    o.inequality_propagation = (InequalityPropagation.On
                                if args.projection_inequality_propagation == "on"
                                else InequalityPropagation.Off)
    o.collect_dedup_stats = False
    o.reachability_filter = (args.projection_reachability_filter == "on")
    return o


def generate_patterns(args, lifted_task):
    backend = args.pattern_gen_backend
    if backend == "cpp-systematic":
        opts = LiftedSystematicPatternGeneratorOptions()
        opts.bounded_fallback = (args.pattern_gen_fallback_bound == "on")
        opts.static_csp = (args.pattern_gen_static_csp == "on")
        opts.reachability = (args.pattern_gen_reachability == "on")
        opts.scorpion_match = (args.pattern_gen_scorpion_match == "on")
        opts.interesting = (args.pattern_gen_interesting == "on")
        opts.max_pattern_size = args.max_pattern_size
        opts.max_pattern_count = args.max_pattern_count
        return LiftedSystematicPatternGenerator(lifted_task, opts).generate()
    if backend == "py-systematic":
        return LiftedPatternGenerator(
            lifted_task,
            bounded_fallback=(args.pattern_gen_fallback_bound == "on"),
            static_csp=(args.pattern_gen_static_csp == "on"),
            reachability=(args.pattern_gen_reachability == "on"),
            scorpion_match=(args.pattern_gen_scorpion_match == "on"),
        ).generate(max_pattern_size=args.max_pattern_size,
                   max_pattern_count=args.max_pattern_count)
    if backend == "py-interesting":
        return LiftedInterestingPatternGenerator(
            lifted_task,
            bounded_fallback=(args.pattern_gen_fallback_bound == "on"),
            static_csp=(args.pattern_gen_static_csp == "on"),
            reachability=(args.pattern_gen_reachability == "on"),
            scorpion_match=(args.pattern_gen_scorpion_match == "on"),
        ).generate(max_pattern_size=args.max_pattern_size,
                   max_pattern_count=args.max_pattern_count)
    if backend == "py-ipdb":
        from lifted_ipdb import LiftedIPDBPatternGenerator
        return LiftedIPDBPatternGenerator(
            lifted_task, num_samples=args.ipdb_samples,
            walk_length=args.ipdb_walk_length,
        ).generate(max_pattern_size=args.max_pattern_size,
                   max_pattern_count=args.max_pattern_count)
    raise RuntimeError(f"unknown backend {backend}")


def main():
    args = build_arg_parser().parse_args()

    patched_domain = maybe_inject_neg_preconds(args.domain_filepath)
    parser = Parser(patched_domain, ParserOptions())
    lifted_task = Task(parser.parse_task(args.task_filepath, ParserOptions()))

    projection_options = make_projection_options(args)
    patterns = generate_patterns(args, lifted_task)

    # ---- the measured region: projection build only ----
    t0 = time.perf_counter_ns()
    projections = ProjectionGenerator(lifted_task, patterns, projection_options).generate()
    t1 = time.perf_counter_ns()

    proj_ms = (t1 - t0) / 1_000_000.0
    print(f"NPAT={len(patterns)}")
    print(f"NPROJ={len(projections)}")
    print(f"PROJ_MS={proj_ms:.4f}")

    if args.sig:
        execution_context = ExecutionContext(1)
        sgen = SuccessorGenerator(lifted_task, execution_context)
        if args.cost_type == "one":
            sgen.set_use_unit_cost(True)
        initial_state = sgen.get_initial_node().get_state()
        per_proj = []
        for proj in projections:
            h = ProjectionAbstractionHeuristic(proj).evaluate(initial_state)
            per_proj.append(h)
        canonical = CanonicalHeuristic(projections).evaluate(initial_state)
        per_proj_sorted = ",".join(f"{v:.3f}" for v in sorted(per_proj))
        print(f"SIG=can={canonical:.3f}|per=[{per_proj_sorted}]")


if __name__ == "__main__":
    main()
