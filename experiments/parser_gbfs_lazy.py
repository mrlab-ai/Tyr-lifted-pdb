#! /usr/bin/env python

from lab.parser import Parser
from lab import tools

import re

def add_coverage(content, props):
    if "plan_length" in props:
        props["coverage"] = 1
    else:
        props["coverage"] = 0

SECTION_MAP = {
    "Successor generator": "succgen",
    "Axiom evaluator": "axiom",
    "FFHeuristic": "ff",
}

RE_SECTION = re.compile(r'^\[(?P<name>[^\]]+)\]\s+Summary$')

RE_PAR_MS  = re.compile(r'^\[ProgramStatistics\].*T_par_region.*:\s*(?P<v>\d+)\s*ms$')
RE_TOT_MS  = re.compile(r'^\[ProgramStatistics\].*T_total.*:\s*(?P<v>\d+)\s*ms$')
RE_FRAC    = re.compile(r'^\[ProgramStatistics\].*Parallel fraction:\s*(?P<v>[0-9]*\.?[0-9]+)$', re.I)

RE_SAMPLES = re.compile(r'^\[AggregatedRuleStatistics\].*Number of samples:\s*(?P<v>\d+)$')
RE_TMIN_MS = re.compile(r'^\[AggregatedRuleStatistics\].*T_min.*:\s*(?P<v>\d+)\s*ms$')
RE_TMAX_MS = re.compile(r'^\[AggregatedRuleStatistics\].*T_max.*:\s*(?P<v>\d+)\s*ms$')
RE_TMED_MS = re.compile(r'^\[AggregatedRuleStatistics\].*T_med.*:\s*(?P<v>\d+)\s*ms$')
RE_SKEW    = re.compile(r'^\[AggregatedRuleStatistics\].*Skew:\s*(?P<v>(?:inf)|(?:[0-9]*\.?[0-9]+))$', re.I)


def parse_datalog_summaries(content, props):
    cur = None  # section prefix like "ff" / "succgen" / "axiom"

    for raw in content.splitlines():
        line = raw.strip()

        m = RE_SECTION.match(line)
        if m:
            name = m.group("name")
            cur = SECTION_MAP.get(name)  # ignore unknown sections
            continue

        if not cur:
            continue

        def put(suffix, value):
            props[f"{cur}_{suffix}"] = value

        m = RE_PAR_MS.match(line)
        if m:
            put("par_ms", int(m.group("v")))
            continue

        m = RE_TOT_MS.match(line)
        if m:
            put("total_ms", int(m.group("v")))
            continue

        m = RE_FRAC.match(line)
        if m:
            put("par_frac", float(m.group("v")))
            continue

        m = RE_SAMPLES.match(line)
        if m:
            put("rule_samples", int(m.group("v")))
            continue

        m = RE_TMIN_MS.match(line)
        if m:
            put("rule_tmin_ms", int(m.group("v")))
            continue

        m = RE_TMAX_MS.match(line)
        if m:
            put("rule_tmax_ms", int(m.group("v")))
            continue

        m = RE_TMED_MS.match(line)
        if m:
            put("rule_tmed_ms", int(m.group("v")))
            continue

        m = RE_SKEW.match(line)
        if m:
            v = m.group("v").lower()
            put("rule_skew", float("inf") if v == "inf" else float(v))
            continue

    # optional derived sequential time outside parallel region (per component)
    for prefix in ("succgen", "axiom", "ff"):
        t = props.get(f"{prefix}_total_ms")
        p = props.get(f"{prefix}_par_ms")
        if t is not None and p is not None:
            props[f"{prefix}_seq_out_ms"] = max(0, t - p)

class GBFSLazyParser(Parser):
    """
    [GBFS] Search started.
    [GBFS] Start node h_value: 9
    [GBFS] New best h_value: 8 with num expanded states 2 and num generated states 9 (0 ms)
    [GBFS] New best h_value: 7 with num expanded states 7 and num generated states 17 (1 ms)
    [GBFS] New best h_value: 6 with num expanded states 9 and num generated states 25 (1 ms)
    [GBFS] New best h_value: 5 with num expanded states 13 and num generated states 32 (2 ms)
    [GBFS] New best h_value: 4 with num expanded states 15 and num generated states 40 (2 ms)
    [GBFS] New best h_value: 3 with num expanded states 17 and num generated states 45 (2 ms)
    [GBFS] New best h_value: 2 with num expanded states 18 and num generated states 46 (3 ms)
    [GBFS] New best h_value: 1 with num expanded states 19 and num generated states 48 (3 ms)
    [GBFS] Search ended.
    [Search] Search time: 3ms
    [Search] Number of generated states: 51
    [Search] Number of expanded states: 20
    [Search] Number of pruned states: 0
    [GBFS] Plan found.
    [GBFS] Plan cost: 13
    [GBFS] Plan length: 13
    pick(ball1 rooma left)
    move(rooma roomb)
    drop(ball1 roomb left)
    move(roomb rooma)
    pick(ball2 rooma left)
    move(rooma roomb)
    drop(ball2 roomb left)
    move(roomb rooma)
    pick(ball3 rooma left)
    pick(ball4 rooma right)
    move(rooma roomb)
    drop(ball4 roomb right)
    drop(ball3 roomb left)

    [Successor generator] Summary
    [ProgramStatistics] T_par_region - wallclock time inside parallel region: 0 ms
    [ProgramStatistics] T_total - wallclock time total: 0 ms
    [ProgramStatistics] Parallel fraction: 1.00
    [AggregatedRuleStatistics] Number of samples: 3
    [AggregatedRuleStatistics] T_min_par_region - minimum wallclock time inside parallel region: 0 ms
    [AggregatedRuleStatistics] T_max_par_region - maximum wallclock time inside parallel region: 0 ms
    [AggregatedRuleStatistics] T_med_par_region - median wallclock time inside parallel region: 0 ms
    [AggregatedRuleStatistics] T_max_par_region / T_med_par_region - Skew: 1.00
    [Axiom evaluator] Summary
    [ProgramStatistics] T_par_region - wallclock time inside parallel region: 0 ms
    [ProgramStatistics] T_total - wallclock time total: 0 ms
    [ProgramStatistics] Parallel fraction: 1.00
    [AggregatedRuleStatistics] Number of samples: 0
    [AggregatedRuleStatistics] T_min_par_region - minimum wallclock time inside parallel region: 0 ms
    [AggregatedRuleStatistics] T_max_par_region - maximum wallclock time inside parallel region: 0 ms
    [AggregatedRuleStatistics] T_med_par_region - median wallclock time inside parallel region: 0 ms
    [AggregatedRuleStatistics] T_max_par_region / T_med_par_region - Skew: 1.00
    [FFHeuristic] Summary
    [ProgramStatistics] T_par_region - wallclock time inside parallel region: 2 ms
    [ProgramStatistics] T_total - wallclock time total: 2 ms
    [ProgramStatistics] Parallel fraction: 1.00
    [AggregatedRuleStatistics] Number of samples: 7
    [AggregatedRuleStatistics] T_min_par_region - minimum wallclock time inside parallel region: 0 ms
    [AggregatedRuleStatistics] T_max_par_region - maximum wallclock time inside parallel region: 0 ms
    [AggregatedRuleStatistics] T_med_par_region - median wallclock time inside parallel region: 0 ms
    [AggregatedRuleStatistics] T_max_par_region / T_med_par_region - Skew: 1.00
    [Total] Peak memory usage: 21311488 bytes
    [Total] Total time: 8 ms
    """
    def __init__(self):
        super().__init__()
        self.add_pattern("plan_cost", r"\[GBFS\] Plan cost: (\d+)", type=int)
        self.add_pattern("plan_length", r"\[GBFS\] Plan length: (\d+)", type=int)

        self.add_pattern("search_time_ms", r"\[Search\] Search time: (\d+) ms", type=int)
        self.add_pattern("expansions", r"\[Search\] Number of expanded states: (\d+)", type=int)
        self.add_pattern("generated", r"\[Search\] Number of generated states: (\d+)", type=int)

        self.add_pattern("total_time_ms", r"\[Total\] Total time: (\d+) ms", type=int)
        self.add_pattern("peak_memory_usage_bytes", r"\[Total\] Peak memory usage: (\d+) bytes", type=int)


        self.add_function(add_coverage)
        self.add_function(parse_datalog_summaries)
        