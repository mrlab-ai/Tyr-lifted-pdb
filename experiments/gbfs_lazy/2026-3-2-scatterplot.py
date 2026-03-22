#! /usr/bin/env python

import json
from pathlib import Path

from lab.experiment import Experiment
from downward.reports.scatter import ScatterPlotReport
from lab.reports import Attribute

### All data points
exp = Experiment("plot-search-time-ms-per-expanded")
exp.add_fetcher("results-combined/2026-1-8-gbfs_lazy-tyr-pl-fd-eval", name="fetch")
exp.add_report(
    ScatterPlotReport(
        attributes=[Attribute("search_time_ms_per_expanded", digits=3)],
        filter_algorithm=["powerlifted-gbfs-lazy-hff-pref-ff", "gbfs-lazy-hff-pref-ff-1"],
        format="png",  # Use "tex" for pgfplots output.
    ),
    name="scatterplot-search-time-ms-per-expanded-png",
)
exp.add_report(
    ScatterPlotReport(
        attributes=[Attribute("search_time_ms_per_expanded", digits=3)],
        filter_algorithm=["powerlifted-gbfs-lazy-hff-pref-ff", "gbfs-lazy-hff-pref-ff-1"],
        format="tex",  # Use "tex" for pgfplots output.
    ),
    name="scatterplot-search-time-ms-per-expanded-tex",
)
exp.run_steps()


TYR_DIR = "results_raw/tyr-2026-1-8-gbfs_lazy-combined-eval"
PL_DIR  = "results_raw/pl-2026-1-9-lazy-gbfs-ff-pref-ff-eval"
COMBINED_DIR  = "results-combined/2026-1-8-gbfs_lazy-tyr-pl-fd-eval"
TYR_ALGO = "gbfs-lazy-hff-pref-ff-1"
PL_ALGO = "powerlifted-gbfs-lazy-hff-pref-ff"

def df(run) -> float:
    total_ms = run["total_time_ns"] / 1_000_000.0
    if total_ms <= 0:
        return 0.0
    ax = float(run.get("axiom_prog_t_tot_ms", 0.0))
    sg = float(run.get("succgen_prog_t_tot_ms", 0.0))
    ff = float(run.get("ff_prog_t_tot_ms", 0.0))
    return (ax + sg + ff) / total_ms

def task_key(run):
    # assumes id = [algorithm, domain, problem, ...]
    return tuple(run["id"][1:])

def build_buckets():
    props = json.load(open(Path(COMBINED_DIR) / "properties"))

    by_task = {}
    for run in props.values():
        algo = run.get("algorithm")
        if algo not in {TYR_ALGO, PL_ALGO}:
            continue
        t = task_key(run)
        by_task.setdefault(t, {})[algo] = run

    allowed_lt_05 = set()
    allowed_ge_05 = set()

    for t, runs in by_task.items():
        # only keep tasks that can actually contribute a scatter point
        if TYR_ALGO not in runs or PL_ALGO not in runs:
            continue

        tyr_run = runs[TYR_ALGO]
        pl_run = runs[PL_ALGO]

        # if TYR is uncovered, there will be no scatter point; skip it
        if tyr_run["coverage"] == 0:
            continue
        if pl_run["coverage"] == 0:
            continue

        if df(tyr_run) < 0.5:
            allowed_lt_05.add(t)
        else:
            allowed_ge_05.add(t)

    return allowed_lt_05, allowed_ge_05

# ---- choose one bucket here ----
allowed_lt_05, allowed_ge_05 = build_buckets()

def make_task_filter(allowed):
    def _f(run):
        return task_key(run) in allowed
    return _f


### df < 0.5
allowed = allowed_lt_05

exp = Experiment("plot-search-time-ms-per-expanded-df-lt-0-5")
task_filter = make_task_filter(allowed)

exp.add_fetcher(COMBINED_DIR, name="fetch-combined", filter=task_filter)

exp.add_report(
    ScatterPlotReport(
        attributes=[Attribute("search_time_ms_per_expanded", digits=3)],
        filter_algorithm=["powerlifted-gbfs-lazy-hff-pref-ff", "gbfs-lazy-hff-pref-ff-1"],
        format="png",
    ),
    name="scatterplot-png",
)
exp.add_report(
    ScatterPlotReport(
        attributes=[Attribute("search_time_ms_per_expanded", digits=3)],
        filter_algorithm=["powerlifted-gbfs-lazy-hff-pref-ff", "gbfs-lazy-hff-pref-ff-1"],
        format="tex",
    ),
    name="scatterplot-tex",
)

exp.run_steps()

### df >= 0.5
allowed = allowed_ge_05

exp = Experiment("plot-search-time-ms-per-expanded-df-ge-0-5")
task_filter = make_task_filter(allowed)

exp.add_fetcher(COMBINED_DIR, name="fetch-combined", filter=task_filter)

exp.add_report(
    ScatterPlotReport(
        attributes=[Attribute("search_time_ms_per_expanded", digits=3)],
        filter_algorithm=["powerlifted-gbfs-lazy-hff-pref-ff", "gbfs-lazy-hff-pref-ff-1"],
        format="png",
    ),
    name="scatterplot-png",
)
exp.add_report(
    ScatterPlotReport(
        attributes=[Attribute("search_time_ms_per_expanded", digits=3)],
        filter_algorithm=["powerlifted-gbfs-lazy-hff-pref-ff", "gbfs-lazy-hff-pref-ff-1"],
        format="tex",
    ),
    name="scatterplot-tex",
)

exp.run_steps()
