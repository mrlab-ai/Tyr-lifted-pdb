#! /usr/bin/env python

"""Parser for the kept-revisions 30-minute experiment.

Captures the per-phase timings (computing patterns, building abstractions,
computing the canonical heuristic, search), peak memory, coverage, and a
phase-aware `error`/outcome string:

    solved
    unsolvable                                  (h = inf at root: proved dead-end)
    out of memory during <phase>
    out of time during <phase>
    error (exit <code>) during <phase>

<phase> is one of: computing patterns, building abstractions,
computing the canonical heuristic, search.

Planner stdout markers (file run.log), each printed when the phase COMPLETES:
    [Total] Pattern generator time: N ms
    [Total] Projection generator time: N ms      (= building abstractions)
    [Total] Canonical heuristic time: N ms
    [Search] Search time: N ms (M ns)
    [ASTAR] Plan found.            -> solved
    [ASTAR] Task is unsolvable!    -> unsolvable (canonical h = inf at root)
    [Total] Peak memory usage: N bytes           (only if the run terminates cleanly)

Failure mode comes from the wrapper exit code (driver.log "exit code: N") and
run.err: bad_alloc / exit 134 -> memory (RLIMIT_AS); SIGKILL (137) with wall time
well below the limit -> memory (cgroup); SIGTERM (143) / wall time at the limit ->
time. The phase is inferred from which completion markers are present.
"""

import re

from lab.parser import Parser
from lab.reports import Attribute, arithmetic_mean, geometric_mean


def _read(path):
    try:
        with open(path) as f:
            return f.read()
    except OSError:
        return ""


def _phase(props):
    # Markers are printed on phase completion, in this order.
    if "time_canonical_ms" in props:
        return "search"
    if "time_abstraction_ms" in props:
        return "computing the canonical heuristic"
    if "time_pattern_ms" in props:
        return "building abstractions"
    return "computing patterns"


def determine_outcome(content, props):
    driver = _read("driver.log")
    err = _read("run.err")

    m = re.search(r"exit code: (-?\d+)", driver)
    exit_code = int(m.group(1)) if m else None
    # Normalize the terminating signal: LocalEnvironment reports negative Python
    # returncodes (-15 = SIGTERM), Slurm/Tetralith reports 128+signal (143).
    signal = None
    if exit_code is not None:
        if exit_code < 0:
            signal = -exit_code
        elif exit_code > 128:
            signal = exit_code - 128
    m = re.search(r"wall-clock time: ([0-9.]+)s", driver)
    wall = float(m.group(1)) if m else None
    limit = props.get("wall_time_limit") or 1800
    phase = _phase(props)

    if "Plan found." in content:
        props["coverage"] = 1
        props["error"] = "solved"
        return
    if "Task is unsolvable!" in content:
        # Canonical heuristic is infinite at the root: proved unsolvable.
        props["coverage"] = 1
        props["error"] = "unsolvable"
        return

    props["coverage"] = 0
    is_mem = signal == 6 or "bad_alloc" in err or "Cannot allocate memory" in err  # SIGABRT
    is_sigkill = signal == 9 or "Killed" in err                                     # SIGKILL
    is_sigterm = signal in (15, 24) or "Terminated" in err                          # SIGTERM/SIGXCPU
    near_limit = wall is not None and wall >= 0.95 * limit

    if is_mem:
        props["error"] = f"out of memory during {phase}"
    elif is_sigterm or (is_sigkill and near_limit):
        props["error"] = f"out of time during {phase}"
    elif is_sigkill:
        # SIGKILL well under the wall limit: cgroup out-of-memory kill.
        props["error"] = f"out of memory during {phase}"
    else:
        props["error"] = f"error (exit {exit_code}) during {phase}"


def add_memory_mib(content, props):
    if "peak_memory_bytes" in props:
        props["memory_mib"] = round(props["peak_memory_bytes"] / (1024 * 1024), 1)


class PhaseOutcomeParser(Parser):
    def __init__(self):
        super().__init__()
        # Per-phase timings (ms). Markers double as "phase completed" flags.
        self.add_pattern("time_pattern_ms", r"Pattern generator time: (\d+) ms", type=int, file="run.log")
        self.add_pattern("time_abstraction_ms", r"Projection generator time: (\d+) ms", type=int, file="run.log")
        self.add_pattern("time_canonical_ms", r"Canonical heuristic time: (\d+) ms", type=int, file="run.log")
        self.add_pattern("search_time_ms", r"Search time: (\d+) ms", type=int, file="run.log")
        self.add_pattern("plan_cost", r"Plan cost: (\d+)", type=int, file="run.log")
        self.add_pattern("plan_length", r"Plan length: (\d+)", type=int, file="run.log")
        self.add_pattern("peak_memory_bytes", r"Peak memory usage: (\d+) bytes", type=int, file="run.log")
        self.add_function(determine_outcome, file="run.log")
        self.add_function(add_memory_mib, file="run.log")

    @staticmethod
    def get_attributes():
        return [
            "run_dir",
            Attribute("coverage", absolute=True, min_wins=False),
            "error",
            Attribute("time_pattern_ms", function=geometric_mean, min_wins=True, digits=1),
            Attribute("time_abstraction_ms", function=geometric_mean, min_wins=True, digits=1),
            Attribute("time_canonical_ms", function=geometric_mean, min_wins=True, digits=1),
            Attribute("search_time_ms", function=geometric_mean, min_wins=True, digits=1),
            Attribute("memory_mib", function=arithmetic_mean, min_wins=True, digits=1),
            "peak_memory_bytes",
            "plan_cost",
            "plan_length",
        ]
