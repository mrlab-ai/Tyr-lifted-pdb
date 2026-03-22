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

from pytyr.formalism.planning import RepositoryFactory, Repository, FDRContext, ObjectBuilder, FluentPredicateBuilder, FluentGroundAtomBuilder, GroundConjunctiveConditionBuilder, DomainBuilder, LiftedTaskBuilder


def main():
    factory = RepositoryFactory()
    repository = factory.create_repository()  # We can also create object directly into the repository of a task.

    object_builder = ObjectBuilder()
    predicate_builder = FluentPredicateBuilder()
    ground_atom_builder = FluentGroundAtomBuilder()
    
    # 1. Re-create existing atom (at ball1 rooma)
    object_builder.name = "ball1"
    ball1, inserted = repository.get_or_create(object_builder)
    assert inserted
    object_builder.name = "rooma"
    rooma, inserted = repository.get_or_create(object_builder)
    assert inserted 

    predicate_builder.name = "at"
    predicate_builder.arity = 2
    at, inserted = repository.get_or_create(predicate_builder)
    assert inserted

    row, inserted = repository.get_or_create(at, [ball1.get_index(), rooma.get_index()])
    assert inserted

    ground_atom_builder.predicate = row.get_index().relation_index
    ground_atom_builder.row = row.get_index().row_index
    at_ball1_rooma, inserted = repository.get_or_create(ground_atom_builder)
    assert inserted

    # 2. Create non existing atom (at ball1 roomb)
    object_builder.name = "roomb"
    roomb, inserted = repository.get_or_create(object_builder)
    assert inserted 

    row, inserted = repository.get_or_create(at, [ball1.get_index(), roomb.get_index()])
    assert inserted 

    ground_atom_builder.predicate = row.get_index().relation_index 
    ground_atom_builder.row = row.get_index().row_index
    at_ball1_roomb, inserted = repository.get_or_create(ground_atom_builder)
    assert inserted

    # 3. Map atom to FDRFact to be passed into state repository for state creations
    fdr_context = FDRContext(repository)
    fact = fdr_context.get_fact(at_ball1_roomb)

    domain_builder = DomainBuilder()
    domain_builder.fluent_predicates.append(at.get_index())

    domain, inserted = repository.get_or_create(domain_builder)

    print(domain)

    goal_builder = GroundConjunctiveConditionBuilder()
    goal, inserted = repository.get_or_create(goal_builder)

    task_builder = LiftedTaskBuilder()
    task_builder.domain = domain.get_index()
    task_builder.goal = goal.get_index()
    task_builder.fluent_atoms.append(at_ball1_rooma.get_index())
    task_builder.fluent_atoms.append(at_ball1_roomb.get_index())

    task, inserted = repository.get_or_create(task_builder)

    print(task)


if __name__ == "__main__":
    main()