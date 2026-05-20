"""
Lifted iPDB pattern generator.

Implements the iPDB hill-climbing procedure (Haslum et al. 2007) adapted for
the lifted setting, using the lifted projection infrastructure provided by Tyr.

Algorithm overview
------------------
1. Initialize with one singleton pattern per positive goal fact.
2. Build a predicate-level causal graph from domain action schemas:
   - pred_predecessors[p] = predicates that causally precede or co-occur with p
     in some action's preconditions, effect conditions, or co-effects.
3. Sample states by unbiased random walks from the initial state.
4. Iteratively hill-climb:
   - For each pattern P and each causally-related neighbor fact f not in P,
     compute the h-value gain of adding f to P over the sampled states.
   - Apply the globally best addition (highest total gain).
   - Stop when no addition improves the mean h-value or all patterns reach
     max_pattern_size.

References
----------
Haslum, P., Botea, A., Helmert, M., Bonet, B., & Koenig, S. (2007).
Domain-independent construction of pattern database heuristics for
cost-optimal planning. In Proc. AAAI-2007.

Example usage (run from the repository root)::

    python3 python/prototypes/lifted_blind.py \\
        -d data/gripper/domain.pddl \\
        -p data/gripper/p-2-0.pddl
"""

import itertools
import random
from collections import defaultdict

from pytyr.formalism.planning import FluentGroundAtomBuilder, FluentPredicateBindingBuilder
from pytyr.planning import Pattern
from pytyr.planning.lifted import (
    Task,
    SuccessorGenerator,
    ProjectionGenerator,
    ProjectionAbstractionHeuristic,
)
from pytyr.common import ExecutionContext


class LiftedIPDBPatternGenerator:
    """
    iPDB hill-climbing pattern generator for lifted planning.

    Builds a predicate-level causal graph and iteratively adds causally
    relevant facts to patterns, guided by h-value improvement on randomly
    sampled states.  All PDB computation uses the lifted projection
    machinery (no full grounding required).
    """

    def __init__(
        self,
        task: Task,
        num_samples: int = 1000,
        walk_length: int = 10,
        random_seed: int = 42,
    ) -> None:
        """
        Parameters
        ----------
        task : Task
            The lifted planning task.
        num_samples : int
            Number of states to sample per evaluation round.
        walk_length : int
            Length of each random walk used for state sampling.
        random_seed : int
            Seed for reproducibility.
        """
        self._task = task
        self._num_samples = num_samples
        self._walk_length = walk_length
        self._rng = random.Random(random_seed)

        # Formalism-level task components
        form_task = task.get_task()
        self._form_task = form_task
        self._fdr_context = task.get_fdr_context()
        self._domain = form_task.get_domain()
        repository = task.get_repository()

        # Enumerate ALL positive FDR facts using variable domains.
        # get_fluent_atoms() only returns initially-true atoms; we must enumerate
        # all valid type-safe ground instantiations via fluent_predicate_domains so
        # that atoms which are initially false (e.g. goal atoms) are included.
        var_domains = task.get_formalism_task().get_variable_domains()
        pred_domains = var_domains.fluent_predicate_domains
        self._all_facts: list = []
        seen_facts: set = set()
        for pred in self._domain.get_fluent_predicates():
            domains = pred_domains.get(pred, [])
            if len(domains) != pred.get_arity():
                continue
            if pred.get_arity() == 0:
                object_tuples = [()]
            else:
                object_tuples = itertools.product(*[d.objects for d in domains])
            for obj_tuple in object_tuples:
                binding, _ = repository.get_or_create(
                    FluentPredicateBindingBuilder(pred, list(obj_tuple))
                )
                atom, _ = repository.get_or_create(FluentGroundAtomBuilder(binding))
                fact = self._fdr_context.get_fact(atom)
                if fact.get_atom() is not None and fact not in seen_facts:
                    self._all_facts.append(fact)
                    seen_facts.add(fact)

        # pred_predecessors[p] = set of FluentPredicates that causally precede p
        self._pred_predecessors: dict = self._build_pred_causal_graph()

        # Index: FluentPredicate -> list of FluentFDRFact
        self._pred_to_facts: dict = defaultdict(list)
        for fact in self._all_facts:
            atom = fact.get_atom()
            if atom is not None:
                self._pred_to_facts[atom.get_predicate()].append(fact)

        # Single-threaded successor generator used only for state sampling
        self._succ_gen = SuccessorGenerator(task, ExecutionContext(1))

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _build_pred_causal_graph(self) -> dict:
        """
        Build the predicate-level causal predecessor graph.

        For every action schema, for every conditional effect e:
        - The action's precondition predicates are predecessors of e's effect predicates.
        - e's condition predicates are predecessors of e's effect predicates.
        - e's effect predicates are mutually predecessors of each other (co-effects).
        """
        pred_predecessors: dict = defaultdict(set)

        for action in self._domain.get_actions():
            precond_preds: set = {
                lit.get_atom().get_predicate()
                for lit in action.get_condition().get_fluent_literals()
            }

            for cond_effect in action.get_effects():
                eff_cond_preds: set = {
                    lit.get_atom().get_predicate()
                    for lit in cond_effect.get_condition().get_fluent_literals()
                }
                effect_preds: set = {
                    lit.get_atom().get_predicate()
                    for lit in cond_effect.get_effect().get_literals()
                }
                all_pred_predecessors = precond_preds | eff_cond_preds

                for p_eff in effect_preds:
                    pred_predecessors[p_eff].update(all_pred_predecessors)
                    # Symmetrically link co-effects
                    pred_predecessors[p_eff].update(effect_preds - {p_eff})

        return pred_predecessors

    def _get_neighbor_facts(self, pattern_facts: list, pattern_facts_set: set) -> list:
        """
        Candidate facts that are causally related to the pattern but not in it.

        A fact f is a neighbor if:
        - f is not already in the pattern, and
        - f's predicate is a causal predecessor of (or co-occurs with) some
          predicate already in the pattern (predicate-level causal graph).

        No object-sharing filter is applied: following standard iPDB, any
        causally-linked fact is a candidate regardless of which objects it
        mentions.  The hill-climbing gain score will select only those additions
        that actually improve the heuristic.
        """
        pattern_preds: set = set()
        for fact in pattern_facts:
            atom = fact.get_atom()
            if atom is not None:
                pattern_preds.add(atom.get_predicate())

        # Neighbor predicates: causal predecessors + same predicates
        neighbor_preds: set = set()
        for p in pattern_preds:
            neighbor_preds.update(self._pred_predecessors.get(p, set()))
        neighbor_preds.update(pattern_preds)

        candidates = []
        seen: set = set()
        for pred in neighbor_preds:
            for fact in self._pred_to_facts.get(pred, []):
                if fact in pattern_facts_set or fact in seen:
                    continue
                if fact.get_atom() is None:
                    continue
                candidates.append(fact)
                seen.add(fact)

        return candidates

    def _sample_states(self) -> list:
        """Sample states by unbiased random walks from the initial state."""
        initial_node = self._succ_gen.get_initial_node()
        states = []
        for _ in range(self._num_samples):
            node = initial_node
            for _ in range(self._walk_length):
                succs = self._succ_gen.get_labeled_successor_nodes(node)
                if not succs:
                    break
                node = self._rng.choice(succs).node
            states.append(node.get_state())
        return states

    def _make_pattern_heuristic(self, facts: list):
        """
        Build a ProjectionAbstractionHeuristic for a single-pattern collection.

        Returns None if projection construction fails.
        """
        try:
            projections = ProjectionGenerator(self._task, [Pattern(facts)]).generate()
            if not projections:
                return None
            return ProjectionAbstractionHeuristic(projections[0])
        except Exception:
            return None

    def _compute_gain(self, new_h, old_h, states: list) -> float:
        """
        Total positive h-value improvement over sampled states.

        Returns Σ_s max(0, h_new(s) − h_old(s)) for states where h_old(s) is
        finite.  States already marked as dead-ends by the old PDB are skipped.
        """
        gain = 0.0
        for state in states:
            h_old = old_h.evaluate(state)
            if h_old == float("inf"):
                continue
            h_new = new_h.evaluate(state)
            delta = h_new - h_old
            if delta > 0.0:
                gain += delta
        return gain

    # ------------------------------------------------------------------
    # Public interface
    # ------------------------------------------------------------------

    def generate(
        self, max_pattern_size: int = 2, max_pattern_count: int = 10
    ) -> list[Pattern]:
        """
        Generate patterns following the iPDB hill-climbing procedure.

        Parameters
        ----------
        max_pattern_size : int
            Maximum number of facts allowed in a single pattern.
        max_pattern_count : int
            Maximum number of patterns (capped at the number of goal facts).

        Returns
        -------
        list[Pattern]
            The generated pattern collection.
        """
        # ── Step 1: initialize one singleton pattern per positive goal fact ──
        goal_facts: list = [
            f for f in self._form_task.get_goal().get_positive_facts()
            if f.get_atom() is not None
        ]

        if not goal_facts:
            print("[iPDB] No positive goal facts; returning empty collection.", flush=True)
            return []

        if len(goal_facts) > max_pattern_count:
            goal_facts = goal_facts[:max_pattern_count]

        # patterns_facts[i] = mutable list of FluentFDRFact for pattern i
        patterns_facts: list[list] = [[f] for f in goal_facts]
        print(f"[iPDB] Initialized {len(patterns_facts)} singleton patterns "
              f"from goal facts.", flush=True)

        if max_pattern_size <= 1:
            return [Pattern(facts) for facts in patterns_facts]

        # ── Step 2: build initial per-pattern PDB heuristics ──
        pattern_heuristics: list = []
        for i, facts in enumerate(patterns_facts):
            h = self._make_pattern_heuristic(facts)
            pattern_heuristics.append(h)
            if h is None:
                print(f"[iPDB] Warning: could not build projection for "
                      f"initial pattern {i}.", flush=True)

        # ── Step 3: sample states ──
        print(f"[iPDB] Sampling {self._num_samples} states "
              f"(walk_length={self._walk_length})...", flush=True)
        sampled_states = self._sample_states()
        print("[iPDB] Sampling complete.", flush=True)

        # ── Step 4: hill-climbing ──
        for iteration in range(1, 10 * max_pattern_size + 1):
            best_gain = 0.0
            best_candidate = None  # (pattern_idx, fact, new_heuristic)

            for i, pat_facts in enumerate(patterns_facts):
                if len(pat_facts) >= max_pattern_size:
                    continue
                old_h = pattern_heuristics[i]
                if old_h is None:
                    continue

                pat_facts_set = set(pat_facts)
                for fact in self._get_neighbor_facts(pat_facts, pat_facts_set):
                    new_h = self._make_pattern_heuristic(pat_facts + [fact])
                    if new_h is None:
                        continue
                    gain = self._compute_gain(new_h, old_h, sampled_states)
                    if gain > best_gain:
                        best_gain = gain
                        best_candidate = (i, fact, new_h)

            if best_candidate is None:
                print(f"[iPDB] No improvement at iteration {iteration}; "
                      "stopping hill-climbing.", flush=True)
                break

            i, fact, new_h = best_candidate
            patterns_facts[i] = patterns_facts[i] + [fact]
            pattern_heuristics[i] = new_h

            sizes = [len(f) for f in patterns_facts]
            print(f"[iPDB] Iter {iteration}: added fact to pattern {i}, "
                  f"gain={best_gain:.4f}, sizes={sizes}", flush=True)

            if all(len(f) >= max_pattern_size for f in patterns_facts):
                print("[iPDB] All patterns reached max size.", flush=True)
                break

        final_patterns = [Pattern(facts) for facts in patterns_facts]
        sizes = [len(f) for f in patterns_facts]
        print(f"[iPDB] Final pattern collection: {len(final_patterns)} patterns, "
              f"sizes={sizes}", flush=True)
        return final_patterns
