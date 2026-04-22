"""Prototype of a lifted iPDB-style pattern generator.
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

YELLOW = "\033[93m"
RED = "\033[91m"
RESET = "\033[0m"


class LiftedIPDBPatternGenerator(PatternGenerator):

    def __init__(self, task: Task) -> None:

        self._task: Task = task
        self._fdr_context = task.get_fdr_context()

    # ------------------------------------------------------------------
    # Helper methods
    # ------------------------------------------------------------------

    """ 
    Check if predicate symbol matches atom symbol.
    """
    def atom_in_predicate(self, atom: FluentGroundAtom, lifted_literal) -> bool:

        lifted_atom = lifted_literal.get_atom()
        return atom.get_predicate().get_name() == lifted_atom.get_predicate().get_name()
    

    """
    Create or retrieve a ground fluent atom pred(objs)
    """
    def create_ground_atom(self, pred: FluentPredicate, objs: List[Object]) -> FluentGroundAtom:
        repository = self._task.get_repository()

        # First create or retrieve the predicate binding ⟨pred, objs⟩.
        binding_builder = FluentPredicateBindingBuilder(pred, objs)
        binding, _ = repository.get_or_create(binding_builder)

        # Then create or retrieve the actual ground atom for that binding.
        atom_builder = FluentGroundAtomBuilder(binding)
        atom, _ = repository.get_or_create(atom_builder)

        

        return atom

    # Try to unify a lifted effect atom with a ground atom.
    #
    # Returns a mapping from ParameterIndex (as int) to Object on success,
    # or ``None`` if the two atoms are incompatible.
    """
    Try to unify a lifted effect atom with a ground atom.
    """
    def unify_effect_atom_with_ground(
        self,
        lifted_atom: FluentAtom,
        ground_atom: FluentGroundAtom,
    ) -> Dict[int, Object] | None:
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

    def object_in_atoms(self, obj: Object, objects) -> bool:
        for o in objects:
            #print("Checking if object", str(obj), "matches", str(o))
            if o == obj:
                return True
        return False

    

    def compatible_type(self, type_mapping, static_atoms, g_atom, obj_tuple: List[Object]) -> bool:

        # Go through static atoms to find all types associated with the objects in the atom,
        # and for each type, collect the set of terms in the mapping that are associated with that type.
        # If they all have at least one term in common, then it is compatible.
        # ex: (at-robby roomb) have types (object roomb) and (room roomb),
        # both of which have the common term V1.

        terms_dict = {}

        # For static atom in static atoms, 
        # if obj in static_atom.get_objects()
        # then for the index of which obj appears in the static atom,
        # add that entry (t)
        for static_atom in static_atoms:
            for obj in obj_tuple:
               #print(type(static_atom))
                #print(atom)
                #print(obj)
                if self.object_in_atoms(obj, static_atom.get_objects()):
                    if(static_atom.get_predicate() not in terms_dict):
                        terms_dict[static_atom.get_predicate()] = set()
                    for term in type_mapping.keys():
                        if static_atom.get_predicate() in type_mapping[term]:
                            terms_dict[static_atom.get_predicate()].add(term)

        # If the intersection of all sets in terms_dict is non-empty, then the object is compatible with the lifted predicate.
        all_terms = set.intersection(*terms_dict.values())

        return len(all_terms) > 0

    """
    Computes all ground atoms that can be generated from a lifted precondition,
    given the constraints of the current parameter-to-object mapping and the available objects for free parameters.
    """
    def compute_available_atoms(self, conditions, lifted_precondition, param_to_obj, available_objects):
        atoms = set()

        lifted_atom = lifted_precondition.get_atom()
        pred = lifted_atom.get_predicate()
        terms = lifted_atom.get_terms()

        static_pre = conditions.get_static_literals()

        type_mappings = {}

        #print("Type mappings for free parameters:", {idx: [t.get_name() for t in types] for idx, types in type_mappings.items()})

        # Use static literals to associate type constraints with the lifted atom's parameters, and filter available_objects accordingly.
        for static_lit in static_pre:
            static_atom = static_lit.get_atom()
            #print("Checking static precondition:", str(static_lit))
            #print("terms in lifted atom:", [str(term) for term in lifted_atom.get_terms()])
            for term in static_atom.get_terms():
                #print("Checking term in static precondition:", str(term))
                if(term not in type_mappings):
                    type_mappings[term] = set()
                    type_mappings[term].add(static_atom.get_predicate().get_name())
                else:
                    type_mappings[term].add(static_atom.get_predicate().get_name())

        #print(f"computed type mappings: { {idx: [pred for pred in preds] for idx, preds in type_mappings.items()} }")

        #print(self._task.get_task().get_static_atoms())
            
            

        #print("Computing available atoms for lifted precondition:", str(lifted_precondition))
        #print(f"static preconditions: {[str(lit) for lit in static_pre]}")

        # Get type constraints from static preconditions and filter available_objects accordingly.
        #for static_lit in static_pre:

        template: list[Object | int] = []
        free_param_indices: list[int] = []


        # This could probably be a helper method.

        #TODO; Also check typing with static literals.



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
            #print("tuplefree:", obj_tuple)
            #print(f"pred, obj_tuple: {str(pred)}, {str(obj_tuple)}")
            #ga = self.create_ground_atom(pred, obj_tuple)
            
            if self.compatible(pred, terms, obj_tuple, type_mappings, self._task.get_task().get_static_atoms()):
                ga = self.create_ground_atom(pred, obj_tuple)
                #print(f"{YELLOW}mainCreated ground atom: {str(ga)}{RESET}")
                atoms.add(ga)
            #else:
                #print(f"{RED}freeIncompatible types for ground atom: {str(ga)}{RESET}")
            return atoms

        # Generate all k-permutations of available objects for k free parameters,
        # and create corresponding ground atoms for each assignment.

        for combo in permutations(available_objects, len(free_param_indices)):
            assignment = dict(zip(free_param_indices, combo))
            obj_tuple: list[Object] = []
            for arg in template:
                if isinstance(arg, int):
                    obj_tuple.append(assignment[arg])
                else:
                    obj_tuple.append(arg)
            #print("tuple:", obj_tuple)
            #print(f"pred, obj_tuple: {str(pred)}, {str(obj_tuple)}")
            #self.compatible(pred, terms, obj_tuple, type_mappings, self._task.get_task().get_static_atoms())
            # Check if the objects in the proposed ground atom satisfy the type constraints derived from static preconditions. If not, skip this combination.
            if self.compatible(pred, terms, obj_tuple, type_mappings, self._task.get_task().get_static_atoms()):
                
                ga = self.create_ground_atom(pred, obj_tuple)
                #print(f"{YELLOW}mainCreated ground atom: {str(ga)}{RESET}")
                atoms.add(ga)
            #else:
                #print(f"{RED}mainIncompatible types for ground atom: {str(pred)}{str(obj_tuple)}{RESET}")

        return atoms

    # Given a predicate and a tuple of objects, check if the combination is compatible with the type constraints derived from static preconditions.
    def compatible(self, pred, terms, obj_tuple, type_mappings, static_atoms) -> bool:
        """
        type_mappings: dict[Term, set[FluentPredicate]]
            Built from static literals; for each Term occurring in a
            static precondition, we store the static predicates (types)
            that constrain that Term.

        static_atoms: sequence of static ground atoms in the task.

        terms: list[Term]
            Terms of the *lifted* precondition atom we are grounding.

        obj_tuple: list[Object]
            Concrete objects proposed for each argument position.
        """

        obj_preds: dict = {}

        allowed_preds: dict[int, set[FluentPredicate]] = {}
        # For each index of the atom, compute allowed types based on static preconditions.
        for pos, term in enumerate(terms):
            tv = term.get_variant()
            if isinstance(tv, ParameterIndex):
                idx = int(tv)
                allowed_preds[term] = type_mappings.get(term)

        for inx, obj in enumerate(obj_tuple):
            obj_preds[obj] = set()
            for static_atom in static_atoms:
                if self.object_in_atoms(obj, static_atom.get_objects()):
                    obj_preds[obj].add(static_atom.get_predicate().get_name())

        #print(terms)
        #print(type_mappings)
        #print(obj_tuple)
        #print(static_atoms)
        #print((obj_preds))
        #print((allowed_preds))

        # For each object index, o in the proposed ground atom, if obj_preds[o] intersect with allowed_preds[terms[index]], the combination is compatible.
        for index, obj in enumerate(obj_tuple):
            if obj_preds[obj] == allowed_preds[terms[index]]:
                #do nothing
                pass
            else:
                #print(f"Combination is NOT compatible for object {obj} at index {index}.")
                return False

        #print("\n")
        return True



    # ------------------------------------------------------------------
    # Causal link computation
    # ------------------------------------------------------------------

    """
    Finds all ground atoms that are causally linked to a given atom via a given lifted action schema.
    """
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
                mapping = self.unify_effect_atom_with_ground(lifted_atom, atom)

                # Successful unification: store mapping if it's new.
                if mapping and mapping not in param_mappings:
                    param_mappings.append(mapping)

        # No unifying effect => no causal link.
        if not param_mappings:
            return atoms
        
        objects = self._task.get_task().get_objects()
        lifted_preconditions = action_schema.get_condition()
        


        for param_to_obj in param_mappings:

            available_objects = [o for o in objects if o not in param_to_obj.values()]

            #We only care about fluents, as static atoms won't help by being included in the pattern.
            for pre_lit in lifted_preconditions.get_fluent_literals():
                lifted_atom = pre_lit.get_atom()
                computed_atoms = self.compute_available_atoms(lifted_preconditions, pre_lit, param_to_obj, available_objects)
                for atom in computed_atoms:
                    if atom not in atoms:
                        atoms.append(atom)


        return atoms

    # ------------------------------------------------------------------
    # BFS tree construction and DOT export
    # ------------------------------------------------------------------

    """
    Construct a causal graph rooted at ``goal_atom`` by backward BFS search,
    exploring all causal links up to a maximum depth.
    """
    def one_pattern_bfs_tree_naive(self, goal_atom: FluentGroundAtom, max_depth: int):
        parent: Dict[FluentGroundAtom, FluentGroundAtom | None] = {goal_atom: None}
        children: Dict[FluentGroundAtom, List[FluentGroundAtom]] = {goal_atom: []}
        depth: Dict[FluentGroundAtom, int] = {goal_atom: 0}

        queue = deque([goal_atom])

        while queue:
            current_atom = queue.popleft()

            if depth[current_atom] >= max_depth:
                continue
            
            for action in self._task.get_task().get_domain().get_actions():
                candidates = self.find_causally_linked_atoms(current_atom, action)
                for candidate in candidates:

                    if candidate == current_atom:
                        continue

                    children.setdefault(current_atom, []).append(candidate)
                    children.setdefault(candidate, [])

                    if candidate in parent:
                        continue  # already discovered; don't re-enqueue

                    parent[candidate] = current_atom
                    depth[candidate] = depth[current_atom] + 1
                    queue.append(candidate)

        return parent, children, depth

    
    """
    Write the causal graph defined by ``parent_map`` and ``children`` to a DOT file for visualization.
    """
    def write_dot(
        self,
        parent_map: Dict[FluentGroundAtom, FluentGroundAtom | None],
        children: Dict[FluentGroundAtom, List[FluentGroundAtom]],
        goal_atom: FluentGroundAtom,
        filename: str,

    ) -> None:

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

    """
    Generate all goal patterns arbitrarily of size max_pattern_size from the causal graph.
    """
    def simple_generation(self, goal_atom, causal_graph, max_pattern_size, max_pattern_count) -> List[Pattern]:

        patterns = []

        pattern_count = 0
        current_pattern = [goal_atom]
        atom_counter = 0
        while pattern_count < max_pattern_count:
            if(len(current_pattern) == max_pattern_size):

                print("Generating pattern with atoms:", [str(atom) for atom in current_pattern])


                facts = [self._fdr_context.get_fact(atom) for atom in current_pattern]
                pattern = Pattern(facts)
                patterns.append(pattern)
                pattern_count += 1
                # Generate next combination of atoms for the pattern (this is just a placeholder, actual logic needed)
                current_pattern = [goal_atom]
            else:
                # get any atom from the causal graph.
                if atom_counter >= len(causal_graph):
                    break  # No more atoms to add, exit loop
                next_atom = list(causal_graph.keys())[atom_counter]
                if next_atom not in current_pattern:
                    current_pattern.append(next_atom)
                atom_counter += 1

        return patterns
    
    """
    Generate a iPDB-style pattern from the causal graph.
    """
    def ipdb_generation(self, goal_atom, causal_graph, max_pattern_size, max_pattern_count) -> List[Pattern]:
        # Placeholder for actual iPDB-style pattern generation logic.
        return self.simple_generation(goal_atom, causal_graph, max_pattern_size, max_pattern_count)
    
    """
    Generate all systematic patterns of size 2, given a goal atom.
    """
    def sys2_generation(self, goal_atom, max_pattern_count) -> List[Pattern]:

        patterns = []

        # For all fluent predicates in the task, for all instantiations (except for those that create that goal atom), store a pattern of size 2 with the goal atom and that fluent atom.
        for predicate in self._task.get_task().get_domain().get_fluent_predicates():
            for obj_tuple in product(self._task.get_task().get_objects(), repeat=predicate.get_arity()):
                candidate_atom = self.create_ground_atom(predicate, list(obj_tuple))
                if candidate_atom != goal_atom:
                    facts = [self._fdr_context.get_fact(goal_atom), self._fdr_context.get_fact(candidate_atom)]
                    pattern = Pattern(facts)
                    patterns.append(pattern)
                    print("Generating sys2 pattern with atoms:", [str(atom) for atom in [goal_atom, candidate_atom]])
                    if len(patterns) >= max_pattern_count:
                        break
            if len(patterns) >= max_pattern_count:
                break

        return patterns
    

    # Generate all systematic patterns of size 1, which is just the pattern containing the goal atom itself.
    def sys1_generation(self, goal_atom) -> List[Pattern]:
        patterns = []
        facts = [self._fdr_context.get_fact(goal_atom)]
        pattern = Pattern(facts)
        patterns.append(pattern)
        #print("Generating sys1 pattern with atoms:", [str(goal_atom)])
        return patterns

    def generate_systematic_patterns(self, goal_atom, max_pattern_size, max_pattern_count) -> List[Pattern]:
        if max_pattern_size == 1:
            return self.sys1_generation(goal_atom)
        elif max_pattern_size == 2:
            return self.sys2_generation(goal_atom, max_pattern_count)
        else:
            # Placeholder for larger systematic patterns.
            return self.simple_generation(goal_atom, {}, max_pattern_size, max_pattern_count)
        

    # Generate all interesting systematic patterns, which uses the causal graph.
    def generate_systematic_interesting_patterns(self, goal_atom, causal_graph, max_pattern_size, max_pattern_count) -> List[Pattern]:
        
        patterns = []
        count = 0

        # Return all patterns of size max_pattern_size that can be formed from the goal atom and its causally linked atoms in the causal graph, up to a maximum count of max_pattern_count.

        if max_pattern_size == 1:
            patterns = self.sys1_generation(goal_atom)
        elif max_pattern_size == 2:
            # Then return all patterns whose non-goal variables has a direct link to the goal atom in the causal graph.
            #print(f"linked atoms of {goal_atom}: {set([str(atom) for atom in causal_graph.get(goal_atom, [])])}")
            for linked_atom in set(causal_graph.get(goal_atom, [])):
                if count >= max_pattern_count:
                    break
                patterns.append(Pattern([self._fdr_context.get_fact(goal_atom), self._fdr_context.get_fact(linked_atom)]))
                #print("Generating systematic interesting pattern with atoms:", [str(atom) for atom in [goal_atom, linked_atom]])
                count += 1
        else:
            # TODO: Logic for interesting sys-k patterns for k > 2 are not part of main baselines, but could be interesting for future exploration.
            return self.simple_generation(goal_atom, causal_graph, max_pattern_size, max_pattern_count)

        return patterns

    """
    Generate a set of interesting patterns from ``goal_atom``,
    using your choice of strategy.
    """
    def generate_interesting_patterns(self, goal_atom, causal_graph, max_pattern_size, max_pattern_count) -> List[Pattern]:
        return self.generate_systematic_interesting_patterns(goal_atom, causal_graph, max_pattern_size, max_pattern_count)


    """
    Compute the causal graph for each goal atom in the task,
    and generate a set of interesting patterns from each goal atom's causal graph.
    """
    def generate(self, max_pattern_size=2, max_pattern_count=10) -> List[Pattern]:
        pattern_collection = []
        causal_graphs = []

        print("Generating patterns with max size", max_pattern_size, "and max count", max_pattern_count)

        

        goals = self._task.get_task().get_goal().get_positive_facts() + self._task.get_task().get_goal().get_negative_facts()

        patterns_count_per_goal = max_pattern_count // len(goals) if goals else 0
        """
        for inx, goal in enumerate(goals):
            goal_atom = goal.get_atom()
            pattern_collection.extend(self.generate_systematic_patterns(goal_atom, max_pattern_size, patterns_count_per_goal))

        """
        # If available, build the causal BFS tree for the first goal atom and
        # dump it as a DOT file for external visualization.
        if goals:
            for inx, goal in enumerate(goals):
                goal_atom = goal.get_atom()
                parent, children, depth = self.one_pattern_bfs_tree_naive(
                    goal_atom,
                    max_depth=max_pattern_size-1,  # Only explore up to max_pattern_size levels in the causal graph, since deeper levels won't be relevant for patterns of that size.
                )
                causal_graphs.append((parent, children, goal_atom))
                #self.write_dot(parent, children, goal_atom, f"goal_{inx}.dot")


        for inx, goal in enumerate(goals):
            for pattern_size in range(1, max_pattern_size + 1):
            #print(f" - {goal.get_atom()}")
                pattern_collection.extend(self.generate_interesting_patterns(
                    goal.get_atom(),
                    causal_graphs[inx][1],  # children_one_pattern_bfs(goal_atom, max_pattern_size, max_pattern_count) dict for this goal atom's BFS tree
                    pattern_size,
                    patterns_count_per_goal,  # Adjust as needed, perhaps pass as command-line arguments
                ))
        
        print(f"Generated {len(pattern_collection)} patterns.")


        return pattern_collection

