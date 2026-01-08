#! /usr/bin/env python

from lab.experiment import Experiment
from downward.reports.absolute import AbsoluteReport

from lab.reports import Attribute, geometric_mean

# Create custom report class with suitable info and error attributes.
class BaseReport(AbsoluteReport):
    INFO_ATTRIBUTES = ["time_limit", "memory_limit"]
    ERROR_ATTRIBUTES = [
        "domain",
        "problem",
        "algorithm",
        "unexplained_errors",
        "error",
        "node",
    ]

ATTRIBUTES = [
    "run_dir",

    Attribute("coverage", min_wins=False),
    # GBFS Lazy
    "plan_cost",
    "plan_length",

    # Search
    "search_time_ms",
    "expansions",
    "generated",

    # Total
    "total_time_ms",
    "peak_memory_usage_bytes",

    # Datalog
    Attribute("axiom_par_frac", function=geometric_mean, min_wins=False),
    "axiom_par_ms",
    "axiom_rule_samples",
    Attribute("axiom_rule_skew", function=geometric_mean, min_wins=False),
    "axiom_rule_tmax_ms",
    "axiom_rule_tmed_ms",
    "axiom_rule_tmin_ms",
    "axiom_seq_out_ms",
    "axiom_total_ms",

    Attribute("ff_par_frac", function=geometric_mean, min_wins=False),
    "ff_par_ms",
    "ff_rule_samples",
    Attribute("ff_rule_skew", function=geometric_mean, min_wins=False),
    "ff_rule_tmax_ms",
    "ff_rule_tmed_ms",
    "ff_rule_tmin_ms",
    "ff_seq_out_ms",
    "ff_total_ms",

    Attribute("succgen_par_frac", function=geometric_mean, min_wins=False),
    "succgen_par_ms",
    "succgen_rule_samples",
    Attribute("succgen_rule_skew", function=geometric_mean, min_wins=False),
    "succgen_rule_tmax_ms",
    "succgen_rule_tmed_ms",
    "succgen_rule_tmin_ms",
    "succgen_seq_out_ms",
    "succgen_total_ms",
]

exp = Experiment("2026-1-8-gbfs_lazy-combined")

exp.add_fetcher("data/2026-1-8-gbfs_lazy-1-eval")
exp.add_fetcher("data/2026-1-8-gbfs_lazy-2-eval")
exp.add_fetcher("data/2026-1-8-gbfs_lazy-4-eval")
exp.add_fetcher("data/2026-1-8-gbfs_lazy-8-eval")

exp.add_report(BaseReport(attributes=ATTRIBUTES))

exp.run_steps()
