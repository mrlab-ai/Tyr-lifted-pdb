#! /usr/bin/env python

from lab.parser import Parser
from lab import tools
from lab.reports import Attribute, geometric_mean, arithmetic_mean

import re


class LiftedPDBParser(Parser):
    """
    """
    def __init__(self):
        super().__init__()
        self.add_pattern("time_ms_pattern_generator", r"\[.*\] Pattern generator time: (\d+)", type=int)
        self.add_pattern("time_ms_projection_generator", r"\[.*\] Projection generator time: (\d+)", type=int)
        self.add_pattern("time_ms_canonical_heuristic", r"\[.*\] Canonical heuristic time: (\d+)", type=int)


        
    @staticmethod
    def get_attributes():
        return [
            Attribute("time_ms_pattern_generator", function=geometric_mean, digits=2),
            Attribute("time_ms_projection_generator", function=geometric_mean, digits=2),
            Attribute("time_ms_canonical_heuristic", function=geometric_mean, digits=2),
        ]
