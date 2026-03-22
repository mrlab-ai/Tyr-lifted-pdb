"""
Create planning formalism structures. 

This example demonstrates how to create planning formalism structures.
- Bindings are currently not exhaustively added.
- We currently match the C++ API and work with indices.

Example usage (run from the repository root):

    python3 python/examples/formalism/planning/builder.py \
        -d data/gripper/domain.pddl \
        -p data/gripper/test_problem.pddl

Author: Dominik Drexler (dominik.drexler@liu.se)
"""

import argparse

from pathlib import Path

from pytyr.formalism.planning import RepositoryFactory, Repository, FDRContext, ObjectBuilder, FluentPredicateBuilder, FluentGroundAtomBuilder, FluentPredicateBindingBuilder, GroundConjunctiveConditionBuilder, DomainBuilder, LiftedTaskBuilder


def main():
    factory = RepositoryFactory()
    repository = factory.create_repository()  # We can also create object directly into the repository of a task.

    ball1, inserted = repository.get_or_create(ObjectBuilder("ball1"))
    assert inserted
    rooma, inserted = repository.get_or_create(ObjectBuilder("rooma"))
    assert inserted 
    roomb, inserted = repository.get_or_create(ObjectBuilder("roomb"))
    assert inserted 

    at, inserted = repository.get_or_create(FluentPredicateBuilder("at", 2))
    assert inserted

    at_ball1_rooma_binding, inserted = repository.get_or_create(FluentPredicateBindingBuilder(at, [ball1, rooma]))
    assert inserted
    at_ball1_rooma, inserted = repository.get_or_create(FluentGroundAtomBuilder(at_ball1_rooma_binding))
    assert inserted

    at_ball1_roomb_binding, inserted = repository.get_or_create(FluentPredicateBindingBuilder(at, [ball1, roomb]))
    assert inserted 
    at_ball1_roomb, inserted = repository.get_or_create(FluentGroundAtomBuilder(at_ball1_roomb_binding))
    assert inserted


if __name__ == "__main__":
    main()