#! /usr/bin/env python

"""Grid experiment: A* + canonical lifted-PDB heuristic over the full HTG suite.

Runs on Tetralith via TetralithEnvironment (NAISS project naiss2025-5-382),
5 minutes wall-time per task. Exercises the projection generator that the
autoresearch loop has been optimizing (current HEAD includes seg2 S2).
"""

import platform
import re
import os
import sys

from pathlib import Path

from downward import suites
from downward.reports.absolute import AbsoluteReport
from lab.environments import TetralithEnvironment, LocalEnvironment
from lab.experiment import Experiment
from lab.reports import Attribute, geometric_mean, arithmetic_mean

DIR = Path(__file__).resolve().parent
REPO = DIR.parent.parent

sys.path.append(str(DIR.parent.parent))

from experiments.parser_datalog import DatalogParser
from experiments.parser_search import SearchParser
from parser_astar_eager import AStarEagerParser
from experiments.lifted_pdb_parser import LiftedPDBParser

from experiments.suite import SUITE_HTG
from experiments.suite_test import SUITE_HTG_TEST


# Create custom report class with suitable info and error attributes.
class BaseReport(AbsoluteReport):
    INFO_ATTRIBUTES = ["wall_time_limit", "memory_limit"]
    ERROR_ATTRIBUTES = [
        "domain",
        "problem",
        "algorithm",
        "unexplained_errors",
        "error",
        "node",
    ]


BENCHMARKS_DIR = Path(os.environ["BENCHMARKS_PDDL"])

NODE = platform.node()
REMOTE = re.match(r"tetralith\d+.nsc.liu.se|n\d+", NODE)

NUM_THREADS = 1
RANDOM_SEED = 0

MEMORY_LIMIT = 2850

# The planner binary links GCC-13.3's libstdc++ at runtime (everything else is
# static). Make the compute-node jobs self-contained by loading that toolchain
# and exporting its lib path, instead of relying on submit-time env inheritance.
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

    SUITES = [
        ("htg-benchmarks-flattened", SUITE_HTG),
    ]
    WALL_TIME_LIMIT = 5 * 60
else:
    ENV = LocalEnvironment(processes=6)

    SUITES = [
        ("htg-benchmarks-flattened", SUITE_HTG_TEST),
    ]
    WALL_TIME_LIMIT = 5 * 60

ATTRIBUTES = [
    "run_dir",
]
ATTRIBUTES += SearchParser.get_attributes()
ATTRIBUTES += AStarEagerParser.get_attributes()
ATTRIBUTES += LiftedPDBParser.get_attributes()

# Create a new experiment.
exp = Experiment(environment=ENV)
exp.add_parser(SearchParser())
exp.add_parser(AStarEagerParser())
exp.add_parser(LiftedPDBParser())

PLANNER_DIR = REPO / "build" / "exe" / "astar_eager"

exp.add_resource("planner_exe", PLANNER_DIR)
exp.add_resource("run_planner", DIR / "astar_eager.sh")

base_cmd = [
    "{run_planner}",
    "{planner_exe}",
    "{domain}",
    "{problem}",
    "plan.out",
    "canonical",
    str(NUM_THREADS),
    str(RANDOM_SEED),
]

for prefix, SUITE in SUITES:
    for task in suites.build_suite(BENCHMARKS_DIR / prefix, SUITE):
        ################ Lifted ################
        run = exp.add_run()
        run.add_resource("domain", task.domain_file, symlink=True)
        run.add_resource("problem", task.problem_file, symlink=True)

        # lab 8.9: wall_time_limit gives a 5-minute wall-clock limit per run.
        run.add_command(
            f"tyr-astar-canonical-lifted-{NUM_THREADS}",
            base_cmd + ["-S"],
            time_limit=None,
            wall_time_limit=WALL_TIME_LIMIT,
            memory_limit=MEMORY_LIMIT,
        )
        # AbsoluteReport needs the following properties:
        # 'domain', 'problem', 'algorithm', 'coverage'.
        run.set_property("domain", task.domain)
        run.set_property("problem", task.problem)
        run.set_property("algorithm", f"tyr-astar-canonical-lifted-{NUM_THREADS}")
        # BaseReport needs the following properties:
        # 'time_limit', 'memory_limit'.
        run.set_property("wall_time_limit", WALL_TIME_LIMIT)
        run.set_property("memory_limit", MEMORY_LIMIT)
        # Every run has to have a unique id in the form of a list.
        # The algorithm name is only really needed when there are
        # multiple algorithms.
        run.set_property("id", [f"tyr-astar-canonical-lifted-{NUM_THREADS}", task.domain, task.problem])

# Add step that writes experiment files to disk.
exp.add_step("build", exp.build)

# Add step that executes all runs.
exp.add_step("start", exp.start_runs)

exp.add_step("parse", exp.parse)

# Add step that collects properties from run directories and
# writes them to *-eval/properties.
exp.add_fetcher(name="fetch")

# Make a report.
exp.add_report(BaseReport(attributes=ATTRIBUTES), outfile="report.html")

# Parse the commandline and run the specified steps.
exp.run_steps()
