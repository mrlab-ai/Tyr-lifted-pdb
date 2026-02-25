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

    "num_objects",

    Attribute("coverage", min_wins=False),
    # GBFS Lazy
    "cost",
    "length",
    "unsolvable",

    # VAL
    "invalid",

    # Search
    "initial_h_value",
    Attribute("search_time", function=geometric_mean),
    "num_expanded",
    "num_generated",
    Attribute("search_time_us_per_expanded", function=geometric_mean),

    # Total
    Attribute("total_time", function=geometric_mean),
    "memory",

    "succgen_prog_n_exec",
    "succgen_prog_t_seq",
    "succgen_prog_t_par",
    "succgen_prog_t_tot",
    "succgen_prog_t_avg",
    Attribute("succgen_prog_pf", function=geometric_mean, min_wins=False),

    "succgen_rule_n_exec",
    "succgen_rule_n_samples",
    "succgen_rule_t_seq",
    "succgen_rule_t_par",
    "succgen_rule_t_tot",
    "succgen_rule_t_avg",
    Attribute("succgen_rule_pf", function=geometric_mean, min_wins=False),
    "succgen_rule_skew_tot",
    "succgen_rule_skew_avg",

    "succgen_rule_worker_n_exec",
    "succgen_rule_worker_n_gen",
    "succgen_rule_worker_n_pen",
    Attribute("succgen_rule_worker_oa", function=geometric_mean, min_wins=False),

    "axiom_prog_n_exec",
    "axiom_prog_t_seq",
    "axiom_prog_t_par",
    "axiom_prog_t_tot",
    "axiom_prog_t_avg",
    Attribute("axiom_prog_pf", function=geometric_mean, min_wins=False),

    "axiom_rule_n_exec",
    "axiom_rule_n_samples",
    "axiom_rule_t_seq",
    "axiom_rule_t_par",
    "axiom_rule_t_tot",
    "axiom_rule_t_avg",
    Attribute("axiom_rule_pf", function=geometric_mean, min_wins=False),
    "axiom_rule_skew_tot",
    "axiom_rule_skew_avg",

    "axiom_rule_worker_n_exec",
    "axiom_rule_worker_n_gen",
    "axiom_rule_worker_n_pen",
    Attribute("axiom_rule_worker_oa", function=geometric_mean, min_wins=False),

    "ff_prog_n_exec",
    "ff_prog_t_seq",
    "ff_prog_t_par",
    "ff_prog_t_tot",
    "ff_prog_t_avg",
    Attribute("ff_prog_pf", function=geometric_mean, min_wins=False),

    "ff_rule_n_exec",
    "ff_rule_n_samples",
    "ff_rule_t_seq",
    "ff_rule_t_par",
    "ff_rule_t_tot",
    "ff_rule_t_avg",
    Attribute("ff_rule_pf", function=geometric_mean, min_wins=False),
    "ff_rule_skew_tot",
    "ff_rule_skew_avg",
    
    "ff_rule_worker_n_exec",
    "ff_rule_worker_n_gen",
    "ff_rule_worker_n_pen",
    Attribute("ff_rule_worker_oa", function=geometric_mean, min_wins=False),
]

# 16 is baseline
exp = Experiment("gbfs_combine_24_1")

def rename_algorithm(properties):
    """Rename algorithm dynamically during fetching."""
    if properties["algorithm"] == "gbfs-lazy-ff-1":
        properties["algorithm"] = "old-gbfs-lazy-ff-1"
        properties["id"][0] = "old-gbfs-lazy-ff-1"
    elif properties["algorithm"] == "gbfs-lazy-ff-2":
        properties["algorithm"] = "old-gbfs-lazy-ff-2"
        properties["id"][0] = "old-gbfs-lazy-ff-2"
    elif properties["algorithm"] == "gbfs-lazy-ff-4":
        properties["algorithm"] = "old-gbfs-lazy-ff-4"
        properties["id"][0] = "old-gbfs-lazy-ff-4"
    elif properties["algorithm"] == "gbfs-lazy-ff-8":
        properties["algorithm"] = "old-gbfs-lazy-ff-8"
        properties["id"][0] = "old-gbfs-lazy-ff-8"
    return properties

exp.add_fetcher("../24-2026-1-8-gbfs_lazy-profiling-classical-combined-eval", filter=rename_algorithm)
exp.add_fetcher("../1-2026-1-8-gbfs_lazy-profiling-classical-combined-eval")

exp.add_report(BaseReport(attributes=ATTRIBUTES, filter_algorithm=[
    "old-gbfs-lazy-ff-1", 
    "gbfs-lazy-ff-1", 
    "old-gbfs-lazy-ff-2", 
    "gbfs-lazy-ff-2", 
    "old-gbfs-lazy-ff-4", 
    "gbfs-lazy-ff-4", 
    "old-gbfs-lazy-ff-8", 
    "gbfs-lazy-ff-8"]))

exp.run_steps()
