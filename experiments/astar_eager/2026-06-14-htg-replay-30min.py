#! /usr/bin/env python

"""HTG experiment for the replay-kept branch, 30-minute limit.

A* + canonical lifted-PDB heuristic (binary built from
autoresearch/replay-kept-2026-06-14). Reports per-phase timings (computing
patterns, building abstractions, computing the canonical heuristic, search),
peak memory, coverage, run_dir, and a phase-aware `error` outcome
(solved / unsolvable / out of memory during X / out of time during X) via the
custom PhaseOutcomeParser.

Local mode runs a handful of tasks with a short limit to validate the pipeline.
"""

import os
import platform
import re
import sys
from pathlib import Path

from downward import suites
from downward.reports.absolute import AbsoluteReport
from lab.environments import TetralithEnvironment, LocalEnvironment
from lab.experiment import Experiment

DIR = Path(__file__).resolve().parent
REPO = DIR.parent.parent
sys.path.append(str(REPO))

from parser_phase_outcome import PhaseOutcomeParser
from experiments.suite import SUITE_HTG


class BaseReport(AbsoluteReport):
    INFO_ATTRIBUTES = ["wall_time_limit", "memory_limit"]
    ERROR_ATTRIBUTES = ["domain", "problem", "algorithm", "unexplained_errors", "error", "node"]


BENCHMARKS_DIR = Path(os.environ["BENCHMARKS_PDDL"])
NODE = platform.node()
# FORCE_LOCAL=1 runs locally even on a Tetralith login node (for the smoke test).
REMOTE = re.match(r"tetralith\d+.nsc.liu.se|n\d+", NODE) and not os.environ.get("FORCE_LOCAL")

NUM_THREADS = 1
RANDOM_SEED = 0
MEMORY_LIMIT = 2850  # MiB, enforced per-run via RLIMIT_AS (-> std::bad_alloc on breach)

GCC13_LIB = "/software/sse2/tetralith_el9/manual/GCC/13.3.0/hpc1/lib64"
TETRALITH_SETUP = (
    f"{TetralithEnvironment.DEFAULT_SETUP}\n"
    "module load buildenv-gcc/13.3.0-bare 2> /dev/null\n"
    f"export LD_LIBRARY_PATH={GCC13_LIB}:$LD_LIBRARY_PATH"
)

if REMOTE:
    # SLURM mem-per-cpu is given headroom above the per-run RLIMIT so the clean
    # per-run bad_alloc (exit 134) fires before the cgroup OOM-killer (exit 137),
    # which would otherwise kill the whole array task and drop sibling runs.
    ENV = TetralithEnvironment(
        setup=TETRALITH_SETUP,
        memory_per_cpu="3600M",
        cpus_per_task=1,
        extra_options="#SBATCH --account=naiss2025-5-382")
    ENV.MAX_TASKS = 300
    SUITES = [("htg-benchmarks-flattened", SUITE_HTG)]
    WALL_TIME_LIMIT = 30 * 60
else:
    ENV = LocalEnvironment(processes=4)
    # Handful of tasks exercising distinct outcomes; short limit for a quick check.
    SUITES = [("htg-benchmarks-flattened", [
        "labyrinth:p01-OPT.pddl",                                  # solves fast
        "blocksworld-large-simple:p-100-2-goal-2.pddl",            # solves
        "organic-synthesis-alkene:p1.pddl",                        # unsolvable (h=inf)
        "rovers-large-simple:p-r1-w1000-o1-1-g2-goal-2.pddl",      # heavy -> OOM/time
    ])]
    WALL_TIME_LIMIT = 120

ATTRIBUTES = PhaseOutcomeParser.get_attributes()

exp = Experiment(environment=ENV)
exp.add_parser(PhaseOutcomeParser())

PLANNER_DIR = REPO / "build" / "exe" / "astar_eager"
exp.add_resource("planner_exe", PLANNER_DIR)
exp.add_resource("run_planner", DIR / "astar_eager.sh")

ALGO = f"replay-canonical-lifted-{NUM_THREADS}"

for prefix, SUITE in SUITES:
    for task in suites.build_suite(BENCHMARKS_DIR / prefix, SUITE):
        run = exp.add_run()
        run.add_resource("domain", task.domain_file, symlink=True)
        run.add_resource("problem", task.problem_file, symlink=True)
        run.add_command(
            ALGO,
            ["{run_planner}", "{planner_exe}", "{domain}", "{problem}",
             "plan.out", "canonical", str(NUM_THREADS), str(RANDOM_SEED), "-S"],
            time_limit=None,
            wall_time_limit=WALL_TIME_LIMIT,
            memory_limit=MEMORY_LIMIT,
        )
        run.set_property("domain", task.domain)
        run.set_property("problem", task.problem)
        run.set_property("algorithm", ALGO)
        run.set_property("wall_time_limit", WALL_TIME_LIMIT)
        run.set_property("memory_limit", MEMORY_LIMIT)
        run.set_property("id", [ALGO, task.domain, task.problem])

# Domains whose tasks use conditional effects (`(when ...)`), which the
# projection/canonical heuristic does not support yet (it can produce a spurious
# h=inf). The planner now aborts on these; they are excluded from the report.
CONDITIONAL_EFFECT_DOMAINS = {"genome-edit-distance-positional"}


def exclude_conditional_effects(run):
    return run["domain"] not in CONDITIONAL_EFFECT_DOMAINS


exp.add_step("build", exp.build)
exp.add_step("start", exp.start_runs)
exp.add_step("parse", exp.parse)
exp.add_fetcher(name="fetch")
exp.add_report(BaseReport(attributes=ATTRIBUTES), outfile="report.html")
exp.add_report(
    BaseReport(attributes=ATTRIBUTES, filter=exclude_conditional_effects),
    outfile="report-no-conditional-effects.html")

exp.run_steps()
