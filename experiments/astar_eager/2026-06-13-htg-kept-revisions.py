#! /usr/bin/env python

"""Compare all kept autoresearch revisions on the full HTG suite.

One algorithm per kept commit (built by build_all_revisions.sh into
experiments/astar_eager/bin/astar_eager-<sha>), A* + canonical lifted-PDB,
5-minute wall limit. Heuristic behaviour is identical across revisions by
construction, so coverage/expansions differ only where the slower (earlier)
revisions exhaust the time budget during projection build; the substantive
signal is time_ms_projection_generator (and total_time_s).
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

from experiments.parser_search import SearchParser
from parser_astar_eager import AStarEagerParser
from experiments.lifted_pdb_parser import LiftedPDBParser

from experiments.suite import SUITE_HTG
from experiments.suite_test import SUITE_HTG_TEST

# Kept revisions in chronological order: (order_index, short_sha, short_label).
REVISIONS = [
    ("00", "484e548", "baseline"),
    ("01", "d32067d", "hashset-dedup"),
    ("02", "48a8cd1", "shared-sigma-domain"),
    ("03", "a14edd9", "fixpoint-fastpath"),
    ("04", "c213312", "hoist-invariants"),
    ("05", "15461fa", "static-ground-hashset"),
    ("06", "db98fde", "arith-sigma-lookup"),
    ("07", "28c77f8", "match-in-place"),
    ("08", "aae4644", "trail-unification"),
    ("09", "4ef995d", "trail-effect-params"),
    ("10", "0d6d3e0", "scratch-atoms"),
    ("11", "ae1078e", "lazy-datalog"),
    ("12", "9d0315c", "linear-patternbitindex"),
    ("13", "97c6e9e", "task-rplus-memo"),
    ("14", "830f4fe", "algorithmic-pruning"),
    ("15", "faafa28", "static-hashjoin"),
    ("16", "c6564f5", "incremental-feasibility"),
]


class BaseReport(AbsoluteReport):
    INFO_ATTRIBUTES = ["wall_time_limit", "memory_limit"]
    ERROR_ATTRIBUTES = [
        "domain", "problem", "algorithm", "unexplained_errors", "error", "node",
    ]


BENCHMARKS_DIR = Path(os.environ["BENCHMARKS_PDDL"])
NODE = platform.node()
REMOTE = re.match(r"tetralith\d+.nsc.liu.se|n\d+", NODE)

NUM_THREADS = 1
RANDOM_SEED = 0
MEMORY_LIMIT = 2850
WALL_TIME_LIMIT = 5 * 60

GCC13_LIB = "/software/sse2/tetralith_el9/manual/GCC/13.3.0/hpc1/lib64"
TETRALITH_SETUP = (
    f"{TetralithEnvironment.DEFAULT_SETUP}\n"
    "module load buildenv-gcc/13.3.0-bare 2> /dev/null\n"
    f"export LD_LIBRARY_PATH={GCC13_LIB}:$LD_LIBRARY_PATH"
)

if REMOTE:
    ENV = TetralithEnvironment(
        setup=TETRALITH_SETUP,
        memory_per_cpu=f"{MEMORY_LIMIT}M",
        cpus_per_task=1,
        extra_options="#SBATCH --account=naiss2025-5-382")
    ENV.MAX_TASKS = 300
    SUITE = SUITE_HTG
else:
    ENV = LocalEnvironment(processes=6)
    SUITE = SUITE_HTG_TEST

ATTRIBUTES = [
    "run_dir",
    *SearchParser.get_attributes(),
    *AStarEagerParser.get_attributes(),
    *LiftedPDBParser.get_attributes(),
]

exp = Experiment(environment=ENV)
exp.add_parser(SearchParser())
exp.add_parser(AStarEagerParser())
exp.add_parser(LiftedPDBParser())

BIN = DIR / "bin"
# One planner resource per revision binary.
for _, sha, _ in REVISIONS:
    exp.add_resource(f"planner_{sha}", BIN / f"astar_eager-{sha}")
exp.add_resource("run_planner", DIR / "astar_eager.sh")

tasks = list(suites.build_suite(BENCHMARKS_DIR / "htg-benchmarks-flattened", SUITE))

for task in tasks:
    for idx, sha, label in REVISIONS:
        algo = f"r{idx}-{label}-{sha}"
        run = exp.add_run()
        run.add_resource("domain", task.domain_file, symlink=True)
        run.add_resource("problem", task.problem_file, symlink=True)
        run.add_command(
            algo,
            ["{run_planner}", f"{{planner_{sha}}}", "{domain}", "{problem}",
             "plan.out", "canonical", str(NUM_THREADS), str(RANDOM_SEED), "-S"],
            time_limit=None,
            wall_time_limit=WALL_TIME_LIMIT,
            memory_limit=MEMORY_LIMIT,
        )
        run.set_property("domain", task.domain)
        run.set_property("problem", task.problem)
        run.set_property("algorithm", algo)
        run.set_property("wall_time_limit", WALL_TIME_LIMIT)
        run.set_property("memory_limit", MEMORY_LIMIT)
        run.set_property("id", [algo, task.domain, task.problem])

exp.add_step("build", exp.build)
exp.add_step("start", exp.start_runs)
exp.add_step("parse", exp.parse)
exp.add_fetcher(name="fetch")
exp.add_report(BaseReport(attributes=ATTRIBUTES), outfile="report.html")

exp.run_steps()
