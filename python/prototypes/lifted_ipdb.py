"""Prototype of a lifted iPDB-style pattern generator.

This module is meant to be used from ``lifted_pdbs.py`` in place of
``GoalPatternGenerator``::

    from python.prototypes.lifted_ipdb import LiftedIPDBPatternGenerator

    patterns = LiftedIPDBPatternGenerator(lifted_task).generate()

Only the interface is defined here; the actual iPDB algorithm is left as
future work.
"""

from __future__ import annotations

from typing import Dict, Iterable, List
from collections import deque

from pytyr.planning import Pattern
from pytyr.planning.lifted import Task, PatternGenerator
from pytyr.formalism.planning import (
    Action,
    FluentGroundAtom,
    FluentLiteral,
    FluentPredicate,
    Object,
    ParameterIndex,
    Variable,
)


class LiftedIPDBPatternGenerator(PatternGenerator):
    """Stub for a lifted iPDB-style pattern generator.

    The interface mirrors :class:`GoalPatternGenerator` so that this class
    can be used as a drop-in replacement in prototypes.  Once implemented,
    ``generate`` should return a list of :class:`pytyr.planning.Pattern
    objects computed using an iPDB-inspired strategy.
    """

    def __init__(self, task: Task) -> None:
        """Create a new pattern generator for ``task``.

        Parameters
        ----------
        task:
            The lifted planning task for which patterns should be generated.
        """

        self._task: Task = task
        # FDR context lets us map ground atoms to FDR facts, which are
        # required by the low-level Pattern representation.
        self._fdr_context = task.get_fdr_context()

    # Check if predicate symbol matches atom symbol.
    def atom_in_predicate(self, atom: FluentGroundAtom, lifted_literal) -> bool:
        """Return True iff ``atom`` matches the predicate of ``lifted_literal``.

        ``lifted_literal`` is a FluentLiteral from a lifted action effect; we
        compare the name of its underlying FluentAtom predicate with the
        predicate of the ground atom.
        """

        lifted_atom = lifted_literal.get_atom()
        return atom.get_predicate().get_name() == lifted_atom.get_predicate().get_name()
    

    # Flag which variables from a condition are already bound by var_to_obj.
    def apply_mapping(
        self,
        variables: Iterable[Variable],
        var_to_obj: Dict[Variable, object],
    ) -> Dict[Variable, bool]:
        """Return a mapping var -> is_free (True if not yet bound).

        This is purely about the variable space of a condition/effect; literals
        themselves are handled by the caller.
        """

        free_variables: Dict[Variable, bool] = {var: True for var in variables}

        for var in var_to_obj:
            if var in free_variables:
                free_variables[var] = False

        return free_variables
    
    # Given an atom and a lifted action whose effects can be unified with the atom,
    # find all *ground* atoms from the preconditions that are causally linked to it.
    def find_causally_linked_atoms(self, atom: FluentGroundAtom, action_schema: Action) -> List[FluentGroundAtom]:
        atoms: List[FluentGroundAtom] = []

        # Single mapping from variables -> objects from the unifying effect literal.
        var_to_obj: Dict[Variable, object] | None = None

        lifted_effects = action_schema.get_effects()
        lifted_preconditions = action_schema.get_condition()

        # Find the mapping that unifies one lifted effect literal with the target atom.
        for cond_eff in lifted_effects:
            effect = cond_eff.get_effect()
            for lifted_literal in effect.get_literals():
                if not self.atom_in_predicate(atom, lifted_literal):
                    continue

                # Build mapping between the conditional effect's variables
                # and the objects in the ground atom.
                var_to_obj = {
                    var: obj
                    for var, obj in zip(cond_eff.get_variables(), atom.get_objects())
                }
                break  # stop after first unifying literal for this cond_eff

            if var_to_obj is not None:
                break  # we've found a unifying effect; no need to look at others

        # No unifying effect => no causal link.
        if var_to_obj is None:
            return atoms

        # Condition variables (same parameter space as the precondition literals).
        cond_vars = lifted_preconditions.get_variables()
        free_flags = self.apply_mapping(cond_vars, var_to_obj)

        objects = self._task.get_task().get_objects()

        # For each lifted precondition literal, partially ground it using var_to_obj
        # and then enumerate remaining free variables over all objects.
        for pre_lit in lifted_preconditions.get_fluent_literals():
            lifted_atom = pre_lit.get_atom()
            terms = lifted_atom.get_terms()

            # Build a template of arguments where each position is either an
            # Object (already grounded) or a Variable (still free to bind).
            template: List[object | Variable] = []
            free_vars_in_lit: List[Variable] = []

            for term in terms:
                tv = term.get_variant()
                if isinstance(tv, Object):
                    template.append(tv)
                elif isinstance(tv, ParameterIndex):
                    # Map ParameterIndex -> Variable using condition's variable list.
                    var = cond_vars[int(tv)]
                    if var in var_to_obj:
                        template.append(var_to_obj[var])
                    else:
                        template.append(var)
                        if free_flags.get(var, True) and var not in free_vars_in_lit:
                            free_vars_in_lit.append(var)
                else:
                    # Fallback: keep as-is.
                    template.append(tv)  # type: ignore[arg-type]

            # Helper to find the corresponding ground atom for a fully grounded tuple.
            def lookup_ground_atom(pred: FluentPredicate, objs: List[Object]) -> FluentGroundAtom | None:
                task = self._task.get_task()
                for ga in task.get_fluent_atoms():
                    if ga.get_predicate() == pred and ga.get_objects() == objs:
                        return ga
                return None

            # Case 1: no free variables in this literal -> directly look up ground atom.
            if not free_vars_in_lit:
                obj_tuple: List[Object] = [arg for arg in template if isinstance(arg, Object)]  # type: ignore[list-item]
                ga = lookup_ground_atom(lifted_atom.get_predicate(), obj_tuple)
                if ga is not None:
                    atoms.append(ga)
                continue

            # Case 2: some variables are still free -> enumerate assignments.
            from itertools import product

            for combo in product(objects, repeat=len(free_vars_in_lit)):
                assignment = dict(zip(free_vars_in_lit, combo))
                obj_tuple: List[Object] = []
                for arg in template:
                    if isinstance(arg, Variable):
                        obj_tuple.append(assignment[arg])
                    else:
                        obj_tuple.append(arg)  # type: ignore[arg-type]

                ga = lookup_ground_atom(lifted_atom.get_predicate(), obj_tuple)
                if ga is not None:
                    atoms.append(ga)

        return atoms

    def _one_pattern_bfs(self, goal_atom, max_pattern_size, max_pattern_count) -> List[Pattern]:
        """Perform a BFS from ``goal_atom`` and return up to ``max_pattern_count`` patterns.

        Each pattern is the ancestor path of some BFS-discovered node, trimmed
        to length at most ``max_pattern_size``. This guarantees that every
        pattern is a weakly connected set of atoms with a directed causal path
        to the goal atom, but patterns themselves are simple paths (no
        branching inside a single pattern).
        """

        if max_pattern_size <= 0 or max_pattern_count <= 0:
            return []

        parent = {goal_atom: None}
        queue = deque([goal_atom])

        patterns: List[Pattern] = []

        while queue and len(patterns) < max_pattern_count:
            current_atom = queue.popleft()

            # Reconstruct ancestor path from root to current_atom as ground
            # atoms.
            atom_path = []
            node = current_atom
            while node is not None:
                atom_path.append(node)
                node = parent[node]
            atom_path.reverse()  # root -> ... -> current_atom

            # Enforce maximum pattern size while keeping a contiguous path
            # that still contains the current_atom.
            if len(atom_path) > max_pattern_size:
                atom_path = atom_path[-max_pattern_size:]

            # Convert ground atoms to FDR facts expected by Pattern.
            fact_path = [self._fdr_context.get_fact(atom) for atom in atom_path]

            patterns.append(Pattern(fact_path))

            if len(patterns) == max_pattern_count:
                break

            # Expand backwards in the causal graph from current_atom.
            for action in self._task.get_task().get_domain().get_actions():
                # find_causally_linked_atoms itself checks whether any of the
                # action's effects unify with current_atom; if not, it returns
                # an empty list.
                candidates = self.find_causally_linked_atoms(current_atom, action)
                for candidate in candidates:
                    if candidate in parent:
                        continue  # already discovered

                    parent[candidate] = current_atom
                    queue.append(candidate)

                if len(patterns) == max_pattern_count:
                    break

        return patterns

    def generate_interesting_patterns(self, goal_atom, max_pattern_size, max_pattern_count) -> List[Pattern]:
        """Generate a set of interesting patterns from ``goal_atom``.

        This now performs a single BFS pass and may return multiple patterns,
        all rooted at the same goal atom and derived from the same BFS tree.
        """

        return self._one_pattern_bfs(goal_atom, max_pattern_size, max_pattern_count)

    def generate(self) -> List[Pattern]:
        """Generate a collection of patterns for ``self._task``.
        This method is intentionally left unimplemented for now and should be
        filled with a lifted iPDB pattern generation algorithm.
        """
        interesting_patterns = []

        goals = self._task.get_task().get_goal().get_fluent_facts()
        print("Goals in the task:")
        for goal in goals:
            print(f" - {goal.get_atom()}")
            interesting_patterns.extend(self.generate_interesting_patterns(
                goal.get_atom(),
                max_pattern_size=3,
                max_pattern_count=100,  # Adjust as needed, perhaps pass as command-line arguments
            ))

        for i, p in enumerate(interesting_patterns[:10]):
            print(f"Pattern {i}: {p}")

        return interesting_patterns

