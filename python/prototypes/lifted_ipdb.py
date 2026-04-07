"""Prototype of a lifted iPDB-style pattern generator.

This module is meant to be used from ``lifted_pdbs.py`` in place of
``GoalPatternGenerator``::

    from python.prototypes.lifted_ipdb import LiftedIPDBPatternGenerator

    patterns = LiftedIPDBPatternGenerator(lifted_task).generate()

Only the interface is defined here; the actual iPDB algorithm is left as
future work.
"""

from __future__ import annotations

import sys
from typing import Dict, Iterable, List
from collections import deque
from itertools import permutations, product

from pytyr.planning import Pattern
from pytyr.planning.lifted import Task, PatternGenerator
from pytyr.formalism.planning import (
    Action,
    FluentAtom,
    FluentGroundAtom,
    FluentGroundAtomBuilder,
    FluentLiteral,
    FluentPredicate,
    FluentPredicateBindingBuilder,
    Object,
    ParameterIndex,
    Variable,
)


class LiftedIPDBPatternGenerator(PatternGenerator):

    def __init__(self, task: Task) -> None:

        self._task: Task = task
        # FDR context lets us map ground atoms to FDR facts, which are
        # required by the low-level Pattern representation.
        self._fdr_context = task.get_fdr_context()

    # Check if predicate symbol matches atom symbol.
    def atom_in_predicate(self, atom: FluentGroundAtom, lifted_literal) -> bool:

        lifted_atom = lifted_literal.get_atom()
        return atom.get_predicate().get_name() == lifted_atom.get_predicate().get_name()
    

    # Flag which variables from a condition are already bound by var_to_obj.
    def apply_mapping(
        self,
        variables: Iterable[Variable],
        var_to_obj: Dict[Variable, object],
    ) -> Dict[Variable, bool]:

        free_variables: Dict[Variable, bool] = {var: True for var in variables}

        for var in var_to_obj:
            if var in free_variables:
                free_variables[var] = False

        return free_variables

    def create_ground_atom(self, pred: FluentPredicate, objs: List[Object]) -> FluentGroundAtom:
        """Create (or retrieve) a ground fluent atom pred(objs) in the task's repository.

        This does *not* require that the atom already appears in ``get_fluent_atoms()``;
        it simply ensures that a corresponding ``FluentGroundAtom`` object exists in
        the shared repository, suitable for use as a node in our causal graph.
        """

        repository = self._task.get_repository()

        # First create or retrieve the predicate binding ⟨pred, objs⟩.
        binding_builder = FluentPredicateBindingBuilder(pred, objs)
        binding, _ = repository.get_or_create(binding_builder)

        # Then create or retrieve the actual ground atom for that binding.
        atom_builder = FluentGroundAtomBuilder(binding)
        atom, _ = repository.get_or_create(atom_builder)

        return atom

    def _unify_effect_atom_with_ground(
        self,
        lifted_atom: FluentAtom,
        ground_atom: FluentGroundAtom,
    ) -> Dict[int, Object] | None:
        """Try to unify a lifted effect atom with a ground atom.

        Returns a mapping from ParameterIndex (as int) to Object on success,
        or ``None`` if the two atoms are incompatible.
        """

        terms = lifted_atom.get_terms()
        objs = ground_atom.get_objects()

        if len(terms) != len(objs):
            return None

        mapping: Dict[int, Object] = {}

        # For each term in the lifted atom, check if it can unify with the
        # corresponding object in the ground atom.
        for i, term in enumerate(terms):
            tv = term.get_variant()
            obj = objs[i]
            if isinstance(tv, Object):
                # Constant in lifted effect must match ground object.
                if tv != obj:
                    return None
            elif isinstance(tv, ParameterIndex):
                idx = int(tv)
                prev = mapping.get(idx)
                if prev is not None and prev != obj:
                    # Same parameter index used in multiple positions must
                    # map to the same object.
                    return None
                mapping[idx] = obj
            else:
                # Unsupported term variant.
                return None

        return mapping
    
    # Given an atom and a lifted action whose effects can be unified with the atom,
    # find all *ground* atoms from the preconditions that are causally linked to it.
    def find_causally_linked_atoms(self, atom: FluentGroundAtom, action_schema: Action) -> List[FluentGroundAtom]:

        atoms: List[FluentGroundAtom] = []

        # All mappings from parameter index (int) to object, derived from
        # unifying effect literals with the target ground atom.
        param_mappings: List[Dict[int, Object]] = []

        lifted_effects = action_schema.get_effects()

        # Go through all of the lifted effect literals and unify them with the ground atom to find.
        for cond_eff in lifted_effects:
            effect = cond_eff.get_effect()
            for lifted_literal in effect.get_literals():
                if not self.atom_in_predicate(atom, lifted_literal):
                    continue

                lifted_atom = lifted_literal.get_atom()  # get lifted atom from literal.
                mapping = self._unify_effect_atom_with_ground(lifted_atom, atom)

                # Successful unification: store mapping if it's new.
                if mapping and mapping not in param_mappings:
                    param_mappings.append(mapping)

        # No unifying effect => no causal link.
        if not param_mappings:
            return atoms
        
        #print(f"Found {len(param_mappings)} unifying effect mappings for atom {atom} and action {action_schema.get_name()}")
        #for i, mapping in enumerate(param_mappings):
            #print(f" - Mapping {i}: {mapping}")

        objects = self._task.get_task().get_objects()
        lifted_preconditions = action_schema.get_condition()
        


        for param_to_obj in param_mappings:
            # Objects that are not yet used in this mapping; free parameters
            # will only be instantiated with these, so we don't reuse objects
            # already fixed by the effect unification (e.g., if bm->b3 and
            # bf->b1, then bt can only range over remaining blocks).
            available_objects = [o for o in objects if o not in param_to_obj.values()]

            #print(f"Available objects for free parameters in preconditions: {available_objects} given mapping {param_to_obj} for atom {atom} and action {action_schema.get_name()}")

            #print(f"Preconditions of action {action_schema.get_name()}:")
            #for pre_lit in lifted_preconditions.get_fluent_literals():
            ##
            #   print(f" - {pre_lit}")

            #print(f"Computed {len(new_atoms)} available atoms from lifted preconditions with mapping {param_to_obj} and available objects {available_objects} for action {action_schema.get_name()}")
            #for new_atom in new_atoms:
            #    print(f" - {new_atom}")

            #We only care about fluents, as static atoms won't help by being included in the pattern.
            for pre_lit in lifted_preconditions.get_fluent_literals():
                lifted_atom = pre_lit.get_atom()
                computed_atoms = self.compute_available_atoms(action_schema.get_variables(), pre_lit, param_to_obj, available_objects)
                for atom in computed_atoms:
                    if atom not in atoms:
                        atoms.append(atom)


        return atoms
    

    # Given a lifted precondition, a partial mapping of variables to objects, and the available objects for free parameters, compute all ground atoms that can be generated from this precondition and mapping.
    def compute_available_atoms(self, _action_variables, lifted_precondition, param_to_obj, available_objects):
        atoms = set()

        lifted_atom = lifted_precondition.get_atom()
        pred = lifted_atom.get_predicate()
        terms = lifted_atom.get_terms()

        template: list[Object | int] = []
        free_param_indices: list[int] = []

        for term in terms:
            tv = term.get_variant()
            if isinstance(tv, Object):
                template.append(tv)
            elif isinstance(tv, ParameterIndex):
                idx = int(tv)
                if idx in param_to_obj:
                    template.append(param_to_obj[idx])   # already grounded
                else:
                    template.append(idx)                 # mark as free
                    if idx not in free_param_indices:
                        free_param_indices.append(idx)
            else:
                return atoms  # unsupported

        if not free_param_indices:
            obj_tuple = [arg for arg in template if isinstance(arg, Object)]
            ga = self.create_ground_atom(pred, obj_tuple)
            atoms.add(ga)
            return atoms

        for combo in permutations(available_objects, len(free_param_indices)):
            assignment = dict(zip(free_param_indices, combo))
            obj_tuple: list[Object] = []
            for arg in template:
                if isinstance(arg, int):
                    obj_tuple.append(assignment[arg])
                else:
                    obj_tuple.append(arg)

            ga = self.create_ground_atom(pred, obj_tuple)
            #print(f"Generated ground atom {ga} from lifted precondition {lifted_precondition} with mapping {param_to_obj} and available objects {available_objects}")
            if ga not in atoms:
                atoms.add(ga)

        return atoms

    def one_pattern_bfs_tree_naive(self, goal_atom: FluentGroundAtom, max_depth: int):
        """Naive BFS tree construction without max pattern size limit.

        This version does not enforce a maximum pattern size and will explore
        the entire backward causal graph reachable from ``goal_atom`` wrt max_depth. It is
        provided for comparison and testing purposes.
        """

        parent: Dict[FluentGroundAtom, FluentGroundAtom | None] = {goal_atom: None}
        children: Dict[FluentGroundAtom, List[FluentGroundAtom]] = {goal_atom: []}
        depth: Dict[FluentGroundAtom, int] = {goal_atom: 0}

        queue = deque([goal_atom])

        while queue:
            current_atom = queue.popleft()
            #print(f"Expanding {current_atom} at depth {depth[current_atom]}")
            #prevent expanding beyond max depth
            if depth[current_atom] >= max_depth:
                continue
            
            for action in self._task.get_task().get_domain().get_actions():
                candidates = self.find_causally_linked_atoms(current_atom, action)
                #print(f"Expanding {current_atom} with action {action.get_name()}, found {len(candidates)} candidates")
                for candidate in candidates:
                    #print(f" - Found candidate: {candidate}")
                    # Skip trivial self-loops in the causal graph. We use
                    # equality rather than identity here because
                    # FluentGroundAtom instances may be re-created but still
                    # represent the same fact.
                    if candidate == current_atom:
                        continue

                    # Always record the causal edge, even if ``candidate``
                    # was discovered earlier. This allows cycles in the
                    # causal graph while still expanding each node at most
                    # once.
                    children.setdefault(current_atom, []).append(candidate)
                    children.setdefault(candidate, [])

                    if candidate in parent:
                        continue  # already discovered; don't re-enqueue

                    parent[candidate] = current_atom
                    depth[candidate] = depth[current_atom] + 1
                    queue.append(candidate)

        return parent, children, depth

    
    def write_dot(
        self,
        parent_map: Dict[FluentGroundAtom, FluentGroundAtom | None],
        children: Dict[FluentGroundAtom, List[FluentGroundAtom]],
        goal_atom: FluentGroundAtom,
        filename: str,

    ) -> None:
        """Write the discovered causal graph to a DOT file.

        Edges are oriented along *causal* direction: from predecessor
        (precondition node) to successor (effect/goal node). We use the
        ``children`` adjacency (which may contain cycles) for edges, and
        ``parent_map`` only to highlight the root goal node(s).
        """

        with open(filename, "w") as f:
            f.write("digraph BFS {\n")

            # Highlight goal node
            print("DOT goal node:", str(goal_atom))
            f.write(f'    "{goal_atom}" [shape=box];\n')

            # Emit all causal edges from predecessors to successors,
            # filtering out self-loops for clarity.
            for succ, preds in children.items():
                for pred in preds:
                    if pred == succ:
                        continue
                    f.write(f'    "{pred}" -> "{succ}";\n')

            f.write("}\n")

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
        #print("Goals in the task:")

        # If available, build the causal BFS tree for the first goal atom and
        # dump it as a DOT file for external visualization.
        if goals:
            first_goal_atom = goals[0].get_atom()
            parent, children, depth = self.one_pattern_bfs_tree_naive(
                first_goal_atom,
                max_depth=1,
            )
            self.write_dot(parent, children, first_goal_atom, "test.dot")

        sys.exit(0)  # Exit after writing the DOT file. Remove this line to run the rest of the pattern generation.

        for goal in goals:
            print(f" - {goal.get_atom()}")
            interesting_patterns.extend(self.generate_interesting_patterns(
                goal.get_atom(),
                max_pattern_size=1,
                max_pattern_count=100,  # Adjust as needed, perhaps pass as command-line arguments
            ))

        for i, p in enumerate(interesting_patterns[:10]):
            print(f"Pattern {i}: {p}")

        return interesting_patterns

