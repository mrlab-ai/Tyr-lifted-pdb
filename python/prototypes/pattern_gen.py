"""
Lifted systematic pattern generator.

Generates all patterns up to a given size by BFS expansion from goal facts
along the predicate-level causal graph, using variable-position filtering
where applicable and a predicate-level fallback otherwise.

Key design decisions
--------------------
1. **Schema-variable-position graph** (primary filter)
   For each action schema we track WHICH parameter indices are shared between
   pairs of predicate positions:

       _pos_neighbors[(pred, pos)] = {(pred2, pos2), ...}

   Two positions (pred, pos) and (pred2, pos2) are neighbors iff the same
   ParameterIndex appears at both positions in at least one action schema.

   This replaces the coarser "any co-occurrence" approach and eliminates
   ground atoms that cannot share objects with the current pattern via any
   action binding.

2. **Positional fact index** for O(1) lookup under the primary filter

       _fact_index[(pred, pos, obj)] → list[FluentFDRFact]

   Given a pattern fact p(a, b), all facts of pred2 that share object a
   at position j are retrieved in O(1).

3. **Predicate-level fallback** for orphaned predicates
   Some PDDL effect atoms introduce a variable that never appears in any
   precondition (e.g., `served(child)` in certain childsnack encodings).
   For such predicates the variable-position graph has no edges, so we fall
   back to including ALL atoms of co-occurring predicates.  This preserves
   correctness at the cost of more candidates, but only for the exceptional
   case.

4. **No projection evaluation during generation**
   Patterns are generated purely from the causal structure.  Unlike iPDB,
   no sampled states or h-value comparisons are needed.

5. **Total count cap** at max_pattern_count
   The BFS returns patterns in ascending size order; generation stops as
   soon as max_pattern_count patterns have been collected.

Example usage (run from the repository root)::

    python3 python/prototypes/lifted_pdbs.py \\
        -d data/childsnack-contents/domain.pddl \\
        -p data/childsnack-contents/contentam1-cham3-p0.pddl \\
        --max-pattern-size 2 --max-pattern-count 10
"""

import itertools
from collections import defaultdict
from typing import DefaultDict

from pytyr.formalism.planning import (
    FluentGroundAtomBuilder,
    FluentPredicateBindingBuilder,
    ParameterIndex,
)
from pytyr.planning import Pattern
from pytyr.planning.lifted import Task


class LiftedPatternGenerator:
    """
    Systematic pattern generator for the lifted setting.

    Enumerates causally connected patterns of size ≤ max_pattern_size starting
    from goal facts.  Variable-position filtering keeps the candidate set small
    in well-structured domains; a predicate-level fallback handles domains
    where some effect predicates have orphaned (unconstrained) variables.
    """

    def __init__(self, task: Task) -> None:
        form_task = task.get_task()
        self._form_task = form_task
        self._fdr_context = task.get_fdr_context()
        self._domain = form_task.get_domain()
        repository = task.get_repository()

        # ── 1. Enumerate all positive FDR facts ──────────────────────────────
        # get_fluent_atoms() returns only initially-true atoms; we use the
        # variable-domain API to enumerate all type-safe ground atoms.
        var_domains = task.get_formalism_task().get_variable_domains()
        pred_domains = var_domains.fluent_predicate_domains
        self._all_facts: list = []
        seen_facts: set = set()
        for pred in self._domain.get_fluent_predicates():
            domains = pred_domains.get(pred, [])
            if len(domains) != pred.get_arity():
                continue
            if pred.get_arity() == 0:
                object_tuples: list = [()]
            else:
                object_tuples = list(itertools.product(*[d.objects for d in domains]))
            for obj_tuple in object_tuples:
                binding, _ = repository.get_or_create(
                    FluentPredicateBindingBuilder(pred, list(obj_tuple))
                )
                atom, _ = repository.get_or_create(FluentGroundAtomBuilder(binding))
                fact = self._fdr_context.get_fact(atom)
                if fact.get_atom() is not None and fact not in seen_facts:
                    self._all_facts.append(fact)
                    seen_facts.add(fact)

        # ── 2. Build causal graphs ────────────────────────────────────────────
        (
            self._pos_neighbors,   # (pred, pos) → {(pred2, pos2)}
            self._pred_cooccur,    # pred → {pred2}  (co-occur in same action)
            self._pred_var_linked, # {(pred1, pred2)} sharing ≥1 schema variable
        ) = self._build_causal_graphs()

        # ── 3. Fact indices ───────────────────────────────────────────────────
        # Primary: (pred, pos, obj) → facts  (for variable-position filter)
        self._fact_index: DefaultDict = defaultdict(list)
        # Fallback: pred → facts            (for orphaned-predicate fallback)
        self._pred_to_facts: DefaultDict = defaultdict(list)
        # Nullary: pred → facts             (0-arity predicates have no positions)
        self._nullary_facts: DefaultDict = defaultdict(list)

        for fact in self._all_facts:
            atom = fact.get_atom()
            if atom is None:
                continue
            pred = atom.get_predicate()
            objs = atom.get_objects()
            if pred.get_arity() == 0:
                self._nullary_facts[pred].append(fact)
            else:
                self._pred_to_facts[pred].append(fact)
                for pos, obj in enumerate(objs):
                    self._fact_index[(pred, pos, obj)].append(fact)

    # ──────────────────────────────────────────────────────────────────────────
    # Graph construction
    # ──────────────────────────────────────────────────────────────────────────

    def _build_causal_graphs(self):
        """
        Scan all action schemas once and produce three data structures.

        Returns
        -------
        pos_neighbors : dict[(pred, int) → set[(pred, int)]]
            Variable-position adjacency: (pred1, pos1) and (pred2, pos2) are
            adjacent iff the same ParameterIndex occupies both positions in
            some action schema.
        pred_cooccur : dict[pred → set[pred]]
            All predicate pairs that co-occur in any action schema (symmetric).
        pred_var_linked : set[frozenset{pred1, pred2}]
            Predicate pairs that share at least one variable (a subset of
            pred_cooccur used to decide between tight and loose neighbor
            extraction).
        """
        pos_neighbors: DefaultDict = defaultdict(set)
        pred_cooccur: DefaultDict = defaultdict(set)
        pred_var_linked: set = set()

        for action in self._domain.get_actions():
            # Collect (pred, pos, var_int) for every fluent atom in this schema
            triples: list = []
            schema_preds: set = set()

            def _harvest(lit_list):
                for lit in lit_list:
                    atom = lit.get_atom()
                    pred = atom.get_predicate()
                    schema_preds.add(pred)
                    for pos, term in enumerate(atom.get_terms()):
                        variant = term.get_variant()
                        if isinstance(variant, ParameterIndex):
                            triples.append((pred, pos, int(variant)))

            _harvest(action.get_condition().get_fluent_literals())
            for ceff in action.get_effects():
                _harvest(ceff.get_condition().get_fluent_literals())
                _harvest(ceff.get_effect().get_literals())

            # Symmetric predicate co-occurrence
            schema_pred_list = list(schema_preds)
            for i, p in enumerate(schema_pred_list):
                for q in schema_pred_list[i + 1:]:
                    pred_cooccur[p].add(q)
                    pred_cooccur[q].add(p)

            # Group triples by variable index; pairs that share a variable
            var_to_positions: DefaultDict = defaultdict(list)
            for pred, pos, var in triples:
                var_to_positions[var].append((pred, pos))

            for positions in var_to_positions.values():
                if len(positions) < 2:
                    continue
                for i, (pred_i, pos_i) in enumerate(positions):
                    for pred_j, pos_j in positions:
                        if (pred_i, pos_i) != (pred_j, pos_j):
                            pos_neighbors[(pred_i, pos_i)].add((pred_j, pos_j))
                # Mark predicate pair as variable-linked.
                # Use `!=` (calls __eq__ via identifying_members) rather than `is`,
                # because the same C++ predicate can be returned through different
                # Python wrapper instances — `is` falsely returns True iff the same
                # wrapper is reused, which we cannot rely on.
                preds_in_group = {pred for pred, _ in positions}
                for p in preds_in_group:
                    for q in preds_in_group:
                        if p != q:
                            pred_var_linked.add(frozenset([p, q]))

        return pos_neighbors, pred_cooccur, pred_var_linked

    # ──────────────────────────────────────────────────────────────────────────
    # Neighbor extraction
    # ──────────────────────────────────────────────────────────────────────────

    def _get_neighbor_facts(self, pattern_facts: list, pattern_facts_set: set) -> list:
        """
        Return candidate ground facts for extending the given pattern.

        For each pair (pattern-predicate, co-occurring predicate):
        - **Variable-linked pair**: use the positional fact index — only atoms
          that share a specific object via a shared schema variable are returned.
          This is O(pattern_size × pattern_arity × position_degree).
        - **Orphaned pair** (co-occur but no shared variable, e.g. certain
          effect predicates): include ALL atoms of the co-occurring predicate.
          Typically rare and involves few atoms.
        - **Nullary predicates**: included whenever their predicate co-occurs
          with any pattern predicate in any action schema.
        """
        candidates: list = []
        seen: set = set()
        pattern_preds: set = set()

        for fact in pattern_facts:
            atom = fact.get_atom()
            if atom is None:
                continue
            pred = atom.get_predicate()
            pattern_preds.add(pred)

            for co_pred in self._pred_cooccur.get(pred, ()):
                if co_pred.get_arity() == 0:
                    continue  # handled after this loop

                if frozenset([pred, co_pred]) in self._pred_var_linked:
                    # Tight filter: use position-based index.
                    # Use `!=` instead of `is not`: PredicateView wrappers do not
                    # preserve Python identity across access paths (different
                    # wrapper instances may refer to the same C++ predicate),
                    # so `is`-based filtering silently drops legitimate
                    # candidates such as (clear b2) for (on b2 b1).
                    for pos, obj in enumerate(atom.get_objects()):
                        for (n_pred, n_pos) in self._pos_neighbors.get((pred, pos), ()):
                            if n_pred != co_pred:
                                continue
                            for candidate in self._fact_index.get((n_pred, n_pos, obj), ()):
                                if candidate in pattern_facts_set or candidate in seen:
                                    continue
                                candidates.append(candidate)
                                seen.add(candidate)
                else:
                    # Fallback: include all atoms of the co-occurring predicate
                    for candidate in self._pred_to_facts.get(co_pred, ()):
                        if candidate in pattern_facts_set or candidate in seen:
                            continue
                        candidates.append(candidate)
                        seen.add(candidate)

        # Nullary predicates co-occurring with any pattern predicate
        for pred in pattern_preds:
            for co_pred in self._pred_cooccur.get(pred, ()):
                if co_pred.get_arity() != 0:
                    continue
                for fact in self._nullary_facts.get(co_pred, ()):
                    if fact in pattern_facts_set or fact in seen:
                        continue
                    candidates.append(fact)
                    seen.add(fact)

        return candidates

    # ──────────────────────────────────────────────────────────────────────────
    # Public interface
    # ──────────────────────────────────────────────────────────────────────────

    def generate(
        self, max_pattern_size: int = 2, max_pattern_count: int = 10
    ) -> list[Pattern]:
        """
        Systematically generate patterns up to max_pattern_size.

        Performs BFS from one singleton pattern per positive goal fact, growing
        each pattern by one causally-connected fact per level.  Duplicate
        patterns (same fact set reached via different expansion paths) are
        suppressed by a frozenset-keyed deduplication set.  Generation stops
        when max_pattern_count patterns have been collected.

        Patterns are returned in ascending size order (singletons first), which
        gives CanonicalHeuristic the strongest individual projections before the
        larger combined ones.

        Parameters
        ----------
        max_pattern_size : int
            Maximum number of facts per pattern.
        max_pattern_count : int
            Maximum total number of patterns returned.

        Returns
        -------
        list[Pattern]
        """
        goal_facts: list = [
            f for f in self._form_task.get_goal().get_positive_facts()
            if f.get_atom() is not None
        ]

        if not goal_facts:
            print("[SysPDB] No positive goal facts; returning empty collection.",
                  flush=True)
            return []

        print(f"[SysPDB] BFS from {len(goal_facts)} goal facts, "
              f"max_size={max_pattern_size}, max_count={max_pattern_count}.",
              flush=True)

        seen_keys: set = set()
        all_fact_lists: list[list] = []

        # Level 0: singleton goal patterns
        current_level: list[list] = []
        for f in goal_facts:
            key = frozenset([f])
            if key in seen_keys:
                continue
            seen_keys.add(key)
            all_fact_lists.append([f])
            current_level.append([f])

        # Level 1 .. max_pattern_size-1: expand by one neighbor fact
        for size in range(2, max_pattern_size + 1):
            if len(all_fact_lists) >= max_pattern_count:
                break
            next_level: list[list] = []
            done = False
            for facts in current_level:
                if done:
                    break
                facts_set = set(facts)
                for neighbor in self._get_neighbor_facts(facts, facts_set):
                    new_facts = facts + [neighbor]
                    key = frozenset(new_facts)
                    if key in seen_keys:
                        continue
                    seen_keys.add(key)
                    all_fact_lists.append(new_facts)
                    next_level.append(new_facts)
                    if len(all_fact_lists) >= max_pattern_count:
                        done = True
                        break
            print(f"[SysPDB] Size-{size}: {len(next_level)} new patterns "
                  f"(total: {len(all_fact_lists)}).", flush=True)
            current_level = next_level
            if not current_level:
                break

        print(f"[SysPDB] Generated {len(all_fact_lists)} patterns.",
              flush=True)
        return [Pattern(facts) for facts in all_fact_lists]
