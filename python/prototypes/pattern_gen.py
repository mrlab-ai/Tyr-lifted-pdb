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
from dataclasses import dataclass
from typing import DefaultDict, Optional

from pytyr.formalism.planning import (
    FluentGroundAtomBuilder,
    FluentPredicateBindingBuilder,
    ParameterIndex,
)
from pytyr.planning import Pattern
from pytyr.planning.lifted import Task


@dataclass
class _StaticLit:
    """A precondition literal of some action schema.

    `arg_terms[i]` is either a `ParameterIndex` (referring to one of the
    action's parameters) or a concrete `Object`. Polarity True means the
    literal is positive (the atom must be present); False means negative
    (it must NOT be). `is_static` selects which relation membership is
    checked against: the task's static atoms (`True`) or the
    delete-relaxation reachable fluent-atom set R^+ (`False`, used only by
    the Phase-6.7 reachability filter, where the literal is a positive
    fluent precondition).
    """
    pred: object                         # predicate
    arg_terms: list                      # list[ParameterIndex | Object]
    polarity: bool
    is_static: bool = True


@dataclass
class _ActionEdge:
    """One precondition→effect coupling within a single action schema.

    Represents: "this action has `pre_pred` at this precondition atom
    *and* `eff_pred` at this effect atom; here is how to map their
    positions to the action's parameters, what static preconditions the
    action carries, and what the typed domain of each parameter is."

    Two ground atoms `g, c` are valid (g, c) under this edge iff there
    exists an assignment of the action's parameters that maps the effect
    atom onto `g`, the precondition atom onto `c`, and satisfies every
    static literal of the action.
    """
    action_name: str                     # for diagnostics only
    eff_param_at_pos: list               # list[int]: pos in effect atom → param index
    pre_param_at_pos: list               # list[int]: pos in pre atom → param index
    static_lits: list                    # list[_StaticLit] (static preconditions)
    param_domains: list                  # list[list[Object]]: typed domain per param
    fluent_pre_lits: list                # list[_StaticLit] (positive fluent preconditions,
                                         # is_static=False; consumed by the R^+ filter)


class LiftedPatternGenerator:
    """
    Systematic pattern generator for the lifted setting.

    Enumerates causally connected patterns of size ≤ max_pattern_size starting
    from goal facts.  Variable-position filtering keeps the candidate set small
    in well-structured domains; a predicate-level fallback handles domains
    where some effect predicates have orphaned (unconstrained) variables.
    """

    # Per-predicate cap used by the bounded fallback path when a co-predicate
    # is effect-only (has no initial-state atoms). 32 was picked as a value
    # that comfortably exceeds the per-predicate fallback count observed in
    # affected domains (childsnack: ≤ 9 effect-only atoms per goal singleton)
    # while clipping rovers-1000-style enumeration of ~1000 reachable atoms.
    _BOUNDED_FALLBACK_EFFECT_ONLY_CAP = 32

    # Wall-time budget (seconds) for the delete-relaxation R^+ fixpoint.
    # If R^+ does not converge within this budget the reachability filter is
    # disabled for the task (the generator falls back to static-CSP-only,
    # which is sound — it merely prunes less). Pure-Python reachability does
    # not scale to the largest HTG instances (logistics-1000 ~ 2 min,
    # rovers-1000 ~ 4 min); the production path is Tyr's native C++ Datalog
    # engine. See docs/lifted_pdb_projection_status.tex.
    _REACHABILITY_TIME_BUDGET_S = 300.0

    def __init__(
        self,
        task: Task,
        *,
        bounded_fallback: bool = True,
        static_csp: bool = True,
        reachability: bool = False,
    ) -> None:
        """
        Parameters
        ----------
        task : Task
            The lifted planning task.
        bounded_fallback : bool, default True
            Phase 6.5 stop-gap. Only consulted when `static_csp=False`.
            When True, co-predicates that share no schema variable with the
            current pattern predicate contribute only their initial-state
            atoms (or up to 32 reachable atoms if effect-only). When False,
            the fallback enumerates every reachable atom of the co-predicate
            — the pre-2026-05-26 behaviour. Exposed via
            `--pattern-gen-fallback-bound` for ablation.
        static_csp : bool, default True
            Phase 6.6 principled fix. When True, every candidate neighbour
            is filtered by a static-precondition feasibility check: only
            candidates for which some action grounding exists that maps the
            effect atom to the goal, the precondition atom to the candidate,
            *and* satisfies every static precondition of that action are
            kept. Subsumes the bounded fallback. When False, falls back to
            the pre-Phase-6.6 schema-variable / fallback split. Exposed via
            `--pattern-gen-static-csp` for ablation.
        reachability : bool, default False
            Phase 6.7 delete-relaxation reachability filter. Only meaningful
            with `static_csp=True`. When True, a candidate is additionally
            required to be reachable: the candidate atom and every fluent
            precondition of the action (under the binding) must lie in the
            delete-relaxation reachable fluent-atom set R^+. This reproduces
            the operator-applicability pruning Scorpion's grounder performs
            (e.g. it drops logistics' airplane patterns, whose precondition
            `at(airplane, non-airport)` is unreachable). R^+ is computed once
            per task by a least-fixpoint over the delete-free action rules.
            NOTE: the pure-Python fixpoint is grounding-level cost and does
            not scale to the largest HTG instances; if it exceeds
            `_REACHABILITY_TIME_BUDGET_S` the filter is silently disabled for
            the task (sound fallback). Default off for that reason; exposed
            via `--pattern-gen-reachability` for opt-in / ablation.
        """
        form_task = task.get_task()
        self._form_task = form_task
        self._fdr_context = task.get_fdr_context()
        self._domain = form_task.get_domain()
        self._repository = task.get_repository()
        self._bounded_fallback = bool(bounded_fallback)
        self._static_csp = bool(static_csp)
        self._reachability = bool(reachability)

        # Cache typed-argument domains per fluent predicate. This is just the
        # type-domain mapping; we do NOT enumerate ground atoms yet. The
        # previous version eagerly built `_all_facts` by Cartesian-product over
        # every fluent predicate's typed-argument domains, which exploded on
        # high-arity predicates with many objects (rovers-1000 has 10^9
        # candidate atoms for arity-3 predicates and never finishes). The
        # tight-filter pattern extension only ever needs atoms matching
        # specific position-object constraints, so we now enumerate those
        # subsets on demand (see `_enumerate_matching`).
        var_domains = task.get_formalism_task().get_variable_domains()
        self._pred_domains = dict(var_domains.fluent_predicate_domains)
        # Per-(action, ceff) parameter-domain table. ParameterIndex values
        # inside an atom are POSITIONAL within a SCOPE: positions 0..arity-1
        # are the action's top-level parameters; positions arity..arity+k-1
        # are the local parameters of the specific ceff the atom belongs to.
        # Different ceffs of the same action can use the same ParameterIndex
        # value to mean different variables — so the CSP must use the
        # param_domains specific to the scope of the eff atom (top-level
        # pre atoms only reference ParameterIndex 0..arity-1, which the
        # ceff's domain covers as its prefix).
        self._scope_param_domains: dict = {}  # (action, ceff_or_None) → dict[int → objs]
        for action, adom in var_domains.action_domains.items():
            arity = action.get_arity()
            pre_vds = adom.precondition_domain.variable_domains
            top_dom = {i: pre_vds[i].objects for i in range(arity)}
            self._scope_param_domains[(action, None)] = top_dom
            for ceff in action.get_effects():
                cdom = adom.effect_domains[ceff].condition_domain.variable_domains
                d = dict(top_dom)
                for j in range(ceff.get_arity()):
                    d[arity + j] = cdom[arity + j].objects
                self._scope_param_domains[(action, ceff)] = d

        # ── Build causal graphs ────────────────────────────────────────────
        # No fact enumeration needed — schema-only analysis.
        (
            self._pos_neighbors,   # (pred, pos) → {(pred2, pos2)}
            self._pred_cooccur,    # pred → {pred2}  (co-occur in same action)
            self._pred_var_linked, # {(pred1, pred2)} sharing ≥1 schema variable
            self._eff_pre_shared_positions,  # (eff_pred, pre_pred) → list[dict[eff_pos→pre_pos]]
            self._action_edges,    # (eff_pred, pre_pred) → list[_ActionEdge]
        ) = self._build_causal_graphs()

        # ── Static-atom index ─────────────────────────────────────────────
        # `(pred → set of (Object, ...) tuples)`. Built from the task's static
        # ground atoms (the union of user-supplied static facts and the
        # implicit type-guard atoms Tyr/Loki generate for each typed
        # parameter). Lookup is O(1) per literal; build is O(|static atoms|).
        self._static_index: DefaultDict = defaultdict(set)
        for atom in form_task.get_static_atoms():
            self._static_index[atom.get_predicate()].add(
                tuple(atom.get_objects())
            )

        # ── Nullary facts ─────────────────────────────────────────────────
        # 0-arity predicates have a single ground atom; enumerate eagerly
        # since there's nothing to product over.
        self._nullary_facts: DefaultDict = defaultdict(list)
        for pred in self._domain.get_fluent_predicates():
            if pred.get_arity() != 0:
                continue
            binding, _ = self._repository.get_or_create(
                FluentPredicateBindingBuilder(pred, [])
            )
            atom, _ = self._repository.get_or_create(
                FluentGroundAtomBuilder(binding)
            )
            fact = self._fdr_context.get_fact(atom)
            if fact.get_atom() is not None:
                self._nullary_facts[pred].append(fact)

        # Lazy cache for `_enumerate_matching` results.
        # Key: (pred, frozenset[(pos, obj)]). Value: list[fact].
        self._enum_match_cache: dict = {}

        # Initial-state fluent facts grouped by predicate. Used by the bounded
        # fallback path (see `_get_neighbor_facts`); always built because the
        # iteration over `get_fluent_atoms()` is linear and cheap.
        self._init_facts_by_pred: DefaultDict = defaultdict(list)
        for atom in form_task.get_fluent_atoms():
            pred = atom.get_predicate()
            fact = self._fdr_context.get_fact(atom)
            if fact.get_atom() is not None:
                self._init_facts_by_pred[pred].append(fact)

        # ── Delete-relaxation reachable fluent atoms R^+ (Phase 6.7) ───────
        # `self._reachable`: dict[pred → set of (Object, ...)] of fluent atoms
        # reachable from the initial state under the delete relaxation, or
        # None if the filter is off / did not converge within the time budget
        # (in which case `_is_valid_candidate` skips the reachability check —
        # a sound fallback that simply prunes less).
        self._reachable = None
        if self._reachability and self._static_csp:
            self._reachable = self._compute_relaxed_reachable(
                self._REACHABILITY_TIME_BUDGET_S)

    # ──────────────────────────────────────────────────────────────────────────
    # Graph construction
    # ──────────────────────────────────────────────────────────────────────────

    def _build_causal_graphs(self):
        """
        Scan all action schemas once and produce five data structures.

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
        eff_pre_shared_positions : dict[(eff_pred, pre_pred) → list[dict[int, int]]]
            Per-action shared-position dicts (Phase 6 pre-CSP path). One
            entry per (effect-atom, precondition-atom) pair within an action
            schema, mapping each effect-atom position to the precondition-atom
            position carrying the same schema variable. Self-pred pairs are
            skipped and entries with no shared positions are omitted.
            Consumed by `_get_neighbor_facts` when `static_csp=False`.
        action_edges : dict[(eff_pred, pre_pred) → list[_ActionEdge]]
            Phase 6.6 enriched action-edge table. One `_ActionEdge` per
            (effect-atom, precondition-atom) pair within an action schema,
            carrying the eff/pre parameter-at-pos maps, the action's static
            preconditions, and the typed parameter domain. Unlike
            `eff_pre_shared_positions`, entries with NO shared parameter
            position are included too — those correspond to the predicate
            co-occurrence the old code routed through the fallback branch,
            and the static-CSP feasibility check disambiguates them
            principled-ly. Self-pred pairs are still skipped (eff-eff
            multi-goal patterns we intentionally don't generate). Consumed
            by `_get_neighbor_facts` when `static_csp=True`.
        """
        pos_neighbors: DefaultDict = defaultdict(set)
        pred_cooccur: DefaultDict = defaultdict(set)
        pred_var_linked: set = set()
        eff_pre_shared: DefaultDict = defaultdict(list)
        action_edges: DefaultDict = defaultdict(list)

        for action in self._domain.get_actions():
            # Collect (pred, pos, var_int) for every fluent atom in this schema
            triples: list = []
            schema_preds: set = set()
            # Per-atom info, tagged with the ceff that owns each atom (None
            # for top-level action precondition). ParameterIndex values are
            # positional within each scope, so pairing must be ceff-aware.
            # Each entry is (pred, pos_vars, param_at_pos, scope_ceff).
            pre_atoms: list = []
            eff_atoms_all: list = []   # all polarities, for legacy eff_pre_shared
            eff_atoms_pos: list = []   # positive-only, for Phase-6.6 action_edges
            top_static_lits: list = []  # action-level statics (top scope)
            ceff_static_lits: dict = {}  # ceff → list[_StaticLit] (extends top scope)
            top_fluent_pre_lits: list = []   # action-level positive fluent precs
            ceff_fluent_pre_lits: dict = {}  # ceff → list[_StaticLit] (fluent, R^+)

            def _harvest_fluent(lit_list, scope_ceff,
                                pre_atoms_out=None,
                                eff_atoms_all_out=None,
                                eff_atoms_pos_out=None):
                """Walk fluent literals.

                Always populates `triples` and `schema_preds`. Appends each
                atom's record to whichever output list(s) are provided:
                  - `pre_atoms_out` for precondition-side atoms (used by
                    both legacy and CSP).
                  - `eff_atoms_all_out` for effect atoms regardless of
                    polarity (used by legacy `eff_pre_shared`, preserving
                    pre-CSP behaviour).
                  - `eff_atoms_pos_out` for POSITIVE effect atoms only
                    (used by Phase-6.6 `action_edges`; negative effects
                    don't establish causal predecessor relations).
                """
                for lit in lit_list:
                    atom = lit.get_atom()
                    pred = atom.get_predicate()
                    schema_preds.add(pred)
                    pos_vars: list = []
                    param_at_pos: list = []
                    for pos, term in enumerate(atom.get_terms()):
                        variant = term.get_variant()
                        if isinstance(variant, ParameterIndex):
                            triples.append((pred, pos, int(variant)))
                            pos_vars.append((pos, int(variant)))
                            param_at_pos.append(int(variant))
                        else:
                            param_at_pos.append(-1)
                    entry = (pred, pos_vars, param_at_pos, scope_ceff)
                    if pre_atoms_out is not None:
                        pre_atoms_out.append(entry)
                    if eff_atoms_all_out is not None:
                        eff_atoms_all_out.append(entry)
                    if eff_atoms_pos_out is not None and lit.get_polarity():
                        eff_atoms_pos_out.append(entry)

            def _harvest_static(lit_list, lits_out):
                for lit in lit_list:
                    atom = lit.get_atom()
                    pred = atom.get_predicate()
                    arg_terms: list = []
                    for term in atom.get_terms():
                        variant = term.get_variant()
                        arg_terms.append(variant)
                    lits_out.append(_StaticLit(pred, arg_terms,
                                              bool(lit.get_polarity())))

            def _harvest_fluent_pre_lits(lit_list, lits_out):
                # Positive fluent precondition literals, recorded as
                # _StaticLit(is_static=False) for the R^+ reachability check.
                for lit in lit_list:
                    if not lit.get_polarity():
                        continue
                    atom = lit.get_atom()
                    arg_terms = [t.get_variant() for t in atom.get_terms()]
                    lits_out.append(_StaticLit(atom.get_predicate(), arg_terms,
                                              True, is_static=False))

            _harvest_fluent(action.get_condition().get_fluent_literals(),
                            None, pre_atoms_out=pre_atoms)
            _harvest_static(action.get_condition().get_static_literals(),
                            top_static_lits)
            _harvest_fluent_pre_lits(action.get_condition().get_fluent_literals(),
                                     top_fluent_pre_lits)
            for ceff in action.get_effects():
                ceff_static_lits[ceff] = []
                ceff_fluent_pre_lits[ceff] = []
                _harvest_fluent(ceff.get_condition().get_fluent_literals(),
                                ceff, pre_atoms_out=pre_atoms)
                _harvest_static(ceff.get_condition().get_static_literals(),
                                ceff_static_lits[ceff])
                _harvest_fluent_pre_lits(ceff.get_condition().get_fluent_literals(),
                                         ceff_fluent_pre_lits[ceff])
                _harvest_fluent(ceff.get_effect().get_literals(),
                                ceff,
                                eff_atoms_all_out=eff_atoms_all,
                                eff_atoms_pos_out=eff_atoms_pos)

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

            # Per-action-edge entries. We pair every effect atom with every
            # precondition atom that lives in the SAME scope: either both in
            # the same ceff, or pre is top-level (whose ParameterIndex range
            # 0..arity-1 is a prefix of every ceff's scope). Pairings across
            # two different ceffs are excluded because their ParameterIndex
            # spaces are not aligned (different ceffs can use the same
            # ParameterIndex value to mean different local variables).
            #
            # The legacy `eff_pre_shared` retains the existing self-pred
            # skip. The Phase-6.6 `action_edges` table does NOT skip
            # self-pred: a predicate can legitimately appear as effect and
            # precondition of the same action (genome-edit-distance:
            # `at(g, w_new)` effect with `at(g, w)` condition, linked by a
            # `transpose-shift`-style static). The static-CSP check filters
            # spurious matches.
            # Legacy `eff_pre_shared`: uses ALL polarities (matches pre-CSP
            # behaviour exactly, including the contributions from negative
            # effects that the original code happened to record).
            for eff_pred, eff_pv, _, _ in eff_atoms_all:
                for pre_pred, pre_pv, _, _ in pre_atoms:
                    if eff_pred == pre_pred:
                        continue
                    shared: dict = {}
                    for eff_pos, eff_var in eff_pv:
                        for pre_pos, pre_var in pre_pv:
                            if eff_var == pre_var:
                                shared[eff_pos] = pre_pos
                                break
                    if shared:
                        eff_pre_shared[(eff_pred, pre_pred)].append(shared)

            # Phase-6.6 `action_edges`: ceff-scope-aware.
            # Polarity note: we iterate eff_atoms_all (BOTH polarities). In
            # Scorpion's binary SAS+ encoding, an operator's "effect" on a
            # variable can be either positive (Atom set) or negative
            # (Atom cleared); both modify the variable and so both establish
            # causal-graph edges. The CSP filter will weed out the cases
            # where a negative-effect edge produces the same atom as the
            # goal (via the candidate==goal seen check) — but it correctly
            # captures patterns like blocksworld {on(b2,b1), clear(b2)},
            # which exists via unstack's negative `on` effect with
            # precondition `clear(?x)`.
            for eff_pred, eff_pv, eff_param_at_pos, eff_ceff in eff_atoms_all:
                edge_statics = top_static_lits + ceff_static_lits[eff_ceff]
                edge_fluent_pres = (top_fluent_pre_lits
                                    + ceff_fluent_pre_lits[eff_ceff])
                edge_param_dom = self._scope_param_domains[(action, eff_ceff)]
                for pre_pred, pre_pv, pre_param_at_pos, pre_ceff in pre_atoms:
                    if pre_ceff is not None and pre_ceff is not eff_ceff:
                        continue  # different ceff scopes — not comparable
                    action_edges[(eff_pred, pre_pred)].append(_ActionEdge(
                        action_name=action.get_name(),
                        eff_param_at_pos=eff_param_at_pos,
                        pre_param_at_pos=pre_param_at_pos,
                        static_lits=edge_statics,
                        param_domains=edge_param_dom,
                        fluent_pre_lits=edge_fluent_pres,
                    ))

        return (pos_neighbors, pred_cooccur, pred_var_linked,
                dict(eff_pre_shared), dict(action_edges))

    # ──────────────────────────────────────────────────────────────────────────
    # Lazy fact enumeration
    # ──────────────────────────────────────────────────────────────────────────

    def _enumerate_matching(self, pred, constraints: dict) -> list:
        """
        Return all facts of `pred` whose argument tuple matches every
        (position → object) entry in `constraints`. Positions not listed in
        `constraints` iterate over the predicate's typed argument domain.

        Result is cached on (pred, frozenset(constraints.items())) so that
        repeated queries — common during BFS expansion when many pattern
        facts share predicate / constraint patterns — pay the enumeration
        cost only once.

        Compared with the previous eager build of `_fact_index`/`_pred_to_facts`
        (which materialised every type-safe ground atom up front), this lazy
        path enumerates only the subset actually queried. For most queries the
        relevant subset is much smaller than the full Cartesian product —
        especially under Step 3's intersection filter, where typically all but
        one position is constrained, so the per-query iteration is O(|domain|)
        rather than O(|domain|^arity).
        """
        key = (pred, frozenset(constraints.items()))
        cached = self._enum_match_cache.get(key)
        if cached is not None:
            return cached

        domains = self._pred_domains.get(pred, [])
        arity = pred.get_arity()
        if len(domains) != arity:
            self._enum_match_cache[key] = []
            return []

        iters = []
        for pos in range(arity):
            if pos in constraints:
                iters.append([constraints[pos]])
            else:
                iters.append(domains[pos].objects)

        facts: list = []
        for obj_tuple in itertools.product(*iters):
            binding, _ = self._repository.get_or_create(
                FluentPredicateBindingBuilder(pred, list(obj_tuple))
            )
            atom, _ = self._repository.get_or_create(
                FluentGroundAtomBuilder(binding)
            )
            fact = self._fdr_context.get_fact(atom)
            if fact.get_atom() is not None:
                facts.append(fact)

        self._enum_match_cache[key] = facts
        return facts

    # ──────────────────────────────────────────────────────────────────────────
    # Static-CSP feasibility (Phase 6.6)
    # ──────────────────────────────────────────────────────────────────────────

    def _eval_static_lit(self, lit, binding):
        """Evaluate a `_StaticLit` under a partial parameter `binding`.

        For static literals (`lit.is_static`) membership is checked against
        the static-atom index; for fluent literals (the R^+ reachability
        filter) it is checked against the delete-relaxation reachable set
        `self._reachable`.

        Returns
        -------
        True  -- the literal is satisfied under any extension of `binding`
                 (all params are bound and the lookup succeeded).
        False -- the literal is violated and the binding cannot be extended
                 to satisfy it.
        None  -- at least one term is unbound; the literal's truth depends
                 on those still-free params (caller must enumerate).
        """
        resolved: list = []
        for t in lit.arg_terms:
            if isinstance(t, ParameterIndex):
                obj = binding.get(int(t))
                if obj is None:
                    return None
                resolved.append(obj)
            else:
                resolved.append(t)
        index = self._static_index if lit.is_static else self._reachable
        present = tuple(resolved) in index.get(lit.pred, set())
        return present if lit.polarity else not present

    def _satisfy_statics(self, binding, static_lits, param_domains):
        """Return True iff `binding` extends to satisfy every literal.

        `static_lits` may mix static literals (checked against the static
        index) and positive fluent-precondition literals (checked against
        R^+); `_eval_static_lit` dispatches per literal. `param_domains` is a
        dict mapping ParameterIndex int values to typed object lists.

        Backtracking search: while there's a still-unbound parameter that
        appears in some unsatisfied literal, branch over its typed domain.
        Already-fully-bound literals are evaluated immediately and prune
        the branch on violation.
        """
        open_lits: list = []
        open_lit_params: list = []
        for lit in static_lits:
            value = self._eval_static_lit(lit, binding)
            if value is True:
                continue
            if value is False:
                return False
            free = {int(t) for t in lit.arg_terms
                    if isinstance(t, ParameterIndex) and int(t) not in binding}
            open_lits.append(lit)
            open_lit_params.append(free)

        if not open_lits:
            return True

        free_counts: DefaultDict = defaultdict(int)
        for free in open_lit_params:
            for p in free:
                free_counts[p] += 1
        if not free_counts:
            return False
        pick = max(free_counts, key=free_counts.get)

        domain = param_domains.get(pick)
        if domain is None:
            return False  # parameter not in scope; malformed input
        for obj in domain:
            new_binding = dict(binding)
            new_binding[pick] = obj
            if self._satisfy_statics(new_binding, open_lits, param_domains):
                return True
        return False

    def _is_valid_candidate(self, goal_fact, candidate_fact,
                            edge: '_ActionEdge') -> bool:
        """Feasibility check for one action-edge.

        True iff some assignment of `edge`'s action parameters maps the
        action's effect atom onto `goal_fact`, its precondition atom onto
        `candidate_fact`, and satisfies every static literal of the action.
        When the delete-relaxation reachability filter is active
        (`self._reachable is not None`), the assignment must additionally
        make every positive fluent precondition of the action — including
        the one matched by the candidate — reachable (a member of R^+). This
        reproduces operator applicability: a candidate is kept only if some
        grounded operator that could create the goal atom can actually fire.
        """
        binding: dict = {}

        # Bind via the effect atom (goal_fact).
        goal_objs = goal_fact.get_atom().get_objects()
        for pos, param_idx in enumerate(edge.eff_param_at_pos):
            if param_idx < 0:
                continue  # constant slot
            obj = goal_objs[pos]
            existing = binding.get(param_idx)
            if existing is not None and existing != obj:
                return False
            binding[param_idx] = obj

        # Bind via the precondition atom (candidate_fact).
        cand_objs = candidate_fact.get_atom().get_objects()
        for pos, param_idx in enumerate(edge.pre_param_at_pos):
            if param_idx < 0:
                continue
            obj = cand_objs[pos]
            existing = binding.get(param_idx)
            if existing is not None and existing != obj:
                return False
            binding[param_idx] = obj

        lits = edge.static_lits
        if self._reachable is not None:
            # Require all positive fluent preconditions to be reachable too.
            lits = lits + edge.fluent_pre_lits
        return self._satisfy_statics(binding, lits, edge.param_domains)

    # ──────────────────────────────────────────────────────────────────────────
    # Delete-relaxation reachability R^+ (Phase 6.7)
    # ──────────────────────────────────────────────────────────────────────────

    def _compute_relaxed_reachable(self, time_budget):
        """Compute the delete-relaxation reachable fluent-atom set R^+.

        Model: one Datalog rule per (action, conditional-effect):
          body = the action's positive preconditions (fluent + static)
                 + the ceff's positive condition literals (fluent + static)
          head = the ceff's positive effect literals
        Negative *static* literals are honoured as filters (constant facts);
        negative *fluent* literals are dropped (standard delete relaxation).
        Seeded with the initial fluent atoms, the least fixpoint is exactly
        the set of atoms reachable under the delete relaxation — the same
        quantity Fast Downward's grounder computes via `instantiate.explore`.

        Returns a dict `pred -> set[(Object, ...)]`, or None if the fixpoint
        does not converge within `time_budget` seconds (sound fallback: the
        caller then skips reachability pruning). The join is indexed (hash
        join) to avoid rescanning large static relations; even so the
        pure-Python evaluation does not scale to the largest HTG instances.
        """
        import time as _time

        # ── Build rules ────────────────────────────────────────────────────
        def atom_terms(atom):
            out = []
            for t in atom.get_terms():
                v = t.get_variant()
                out.append(int(v) if isinstance(v, ParameterIndex) else v)
            return out

        rules = []   # (pos_body, neg_static_body, head)
        for action in self._domain.get_actions():
            top_pos, top_neg = [], []

            def harvest(cond, pos_out, neg_out):
                for lit in cond.get_static_literals():
                    a = lit.get_atom()
                    if lit.get_polarity():
                        pos_out.append((a.get_predicate(), atom_terms(a), True))
                    else:
                        neg_out.append((a.get_predicate(), atom_terms(a)))
                for lit in cond.get_fluent_literals():
                    if lit.get_polarity():
                        a = lit.get_atom()
                        pos_out.append((a.get_predicate(), atom_terms(a), False))

            harvest(action.get_condition(), top_pos, top_neg)
            for ceff in action.get_effects():
                cpos, cneg = list(top_pos), list(top_neg)
                harvest(ceff.get_condition(), cpos, cneg)
                head = [(lit.get_atom().get_predicate(),
                         atom_terms(lit.get_atom()))
                        for lit in ceff.get_effect().get_literals()
                        if lit.get_polarity()]
                if head:
                    rules.append((cpos, cneg, head))

        # ── Seed R^+ with the initial fluent atoms ─────────────────────────
        R: DefaultDict = defaultdict(set)
        for atom in self._form_task.get_fluent_atoms():
            R[atom.get_predicate()].add(tuple(atom.get_objects()))

        static_lists = {p: list(s) for p, s in self._static_index.items()}

        class _IndexCache:
            """Lazily hash-index a relation (list of tuples) by a position set."""
            def __init__(self):
                self.cache = {}

            def lookup(self, rel, positions, keyvals):
                rid = id(rel)
                per = self.cache.setdefault(rid, {})
                key = tuple(sorted(positions))
                idx = per.get(key)
                if idx is None:
                    idx = defaultdict(list)
                    for tup in rel:
                        idx[tuple(tup[p] for p in key)].append(tup)
                    per[key] = idx
                return idx.get(tuple(keyvals[p] for p in key), ())

        static_ic = _IndexCache()  # static relations never change across rounds

        t0 = _time.perf_counter()
        R_frozen: dict = {}

        def rel_list(pred, is_static):
            return static_lists.get(pred, ()) if is_static else R_frozen.get(pred, ())

        state = {"changed": True}

        def fire(pos_body, neg_body, head, fluent_ic):
            def rec(remaining, binding):
                if not remaining:
                    for pred, terms in neg_body:
                        if any(isinstance(t, int) and t not in binding for t in terms):
                            continue  # unbound negative: optimistically skip
                        resolved = tuple(binding[t] if isinstance(t, int) else t
                                         for t in terms)
                        if resolved in self._static_index.get(pred, ()):
                            return  # negative static violated
                    for pred, terms in head:
                        atom = tuple(binding[t] if isinstance(t, int) else t
                                     for t in terms)
                        if atom not in R[pred]:
                            R[pred].add(atom)
                            state["changed"] = True
                    return
                # pick the most-bound conjunct (then smallest relation)
                def bound_positions(c):
                    _, terms, _ = c
                    return [i for i, t in enumerate(terms)
                            if not isinstance(t, int) or t in binding]
                best = max(remaining,
                           key=lambda c: (len(bound_positions(c)),
                                          -len(rel_list(c[0], c[2]))))
                rem2 = [c for c in remaining if c is not best]
                pred, terms, is_static = best
                rel = rel_list(pred, is_static)
                bp = bound_positions(best)
                if bp:
                    keyvals = {i: (binding[terms[i]] if isinstance(terms[i], int)
                                   else terms[i]) for i in bp}
                    ic = static_ic if is_static else fluent_ic
                    cand = ic.lookup(rel, bp, keyvals)
                else:
                    cand = rel
                for tup in cand:
                    if len(tup) != len(terms):
                        continue
                    added = []
                    ok = True
                    for i, t in enumerate(terms):
                        if isinstance(t, int):
                            if t in binding:
                                if binding[t] != tup[i]:
                                    ok = False
                                    break
                            else:
                                binding[t] = tup[i]
                                added.append(t)
                        elif t != tup[i]:
                            ok = False
                            break
                    if ok:
                        rec(rem2, binding)
                    for k in added:
                        del binding[k]
            rec(list(pos_body), {})

        rounds = 0
        while state["changed"]:
            if _time.perf_counter() - t0 > time_budget:
                print(f"[PATTERN] reachability: R+ did not converge within "
                      f"{time_budget:.0f}s ({rounds} rounds); disabling the "
                      f"reachability filter for this task (sound fallback).",
                      flush=True)
                return None
            state["changed"] = False
            rounds += 1
            # freeze fluent relations for this round; heads write to live R
            R_frozen.clear()
            for pred, s in R.items():
                R_frozen[pred] = tuple(s)
            fluent_ic = _IndexCache()
            for pos_body, neg_body, head in rules:
                fire(pos_body, neg_body, head, fluent_ic)

        total = sum(len(s) for s in R.values())
        print(f"[PATTERN] reachability: R+ converged in {rounds} rounds, "
              f"{total} reachable fluent atoms "
              f"({(_time.perf_counter()-t0)*1000:.0f} ms).", flush=True)
        return dict(R)

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

            # In the CSP path we also iterate self-pred edges (e.g. genome
            # `at` appears as both effect and precondition of `transpose`,
            # establishing a real (at, at) causal edge). `pred_cooccur` is
            # built over distinct predicate pairs only, so we union in self
            # when an action-edge actually exists.
            if self._static_csp and (pred, pred) in self._action_edges:
                co_preds = list(self._pred_cooccur.get(pred, ())) + [pred]
            else:
                co_preds = self._pred_cooccur.get(pred, ())

            for co_pred in co_preds:
                if co_pred.get_arity() == 0:
                    continue  # handled after this loop

                if self._static_csp:
                    # Phase 6.6 principled path: for every action-edge that
                    # couples `pred` (as effect) to `co_pred` (as precondition),
                    # enumerate candidates compatible with the schema-variable
                    # bindings AND validate each against the action's static
                    # preconditions via `_is_valid_candidate`. This subsumes
                    # both the var-linked tight branch and the
                    # no-shared-variable fallback below.
                    #
                    # We deliberately only use the predecessor direction
                    # (goal as eff, candidate as pre). Adding a symmetric
                    # successor direction (Pommerening's interesting-pattern
                    # filter is symmetric) finds some additional Scorpion
                    # patterns in genome-edit-distance but introduces
                    # operator-reachability over-counts in pipesworld and
                    # logistics. The asymmetric form matches Scorpion on
                    # more HTG domains overall.
                    for edge in self._action_edges.get((pred, co_pred), ()):
                        goal_objs = atom.get_objects()
                        goal_binding = {
                            p: goal_objs[pos]
                            for pos, p in enumerate(edge.eff_param_at_pos)
                            if p >= 0
                        }
                        constraints = {
                            pre_pos: goal_binding[p]
                            for pre_pos, p in enumerate(edge.pre_param_at_pos)
                            if p in goal_binding
                        }
                        for candidate in self._enumerate_matching(
                                co_pred, constraints):
                            if candidate in pattern_facts_set \
                                    or candidate in seen:
                                continue
                            if not self._is_valid_candidate(
                                    fact, candidate, edge):
                                continue
                            candidates.append(candidate)
                            seen.add(candidate)
                    continue  # skip the legacy var-linked/fallback split

                if frozenset([pred, co_pred]) in self._pred_var_linked:
                    # Tight filter: action-aware per-edge intersection
                    # (Step 3 of the action-aware refactor). For each
                    # action-edge that links `pred` (as effect) to `co_pred`
                    # (as precondition), produce only those `co_pred`-facts
                    # that match the goal fact at ALL shared positions
                    # simultaneously — by intersecting the existing
                    # single-position fact-index sets across the shared
                    # positions of that action-edge.
                    #
                    # This replaces the previous per-position UNION, which was
                    # structurally too loose for high-arity fluent predicates
                    # (visitall-multidim 14×–665× over Scorpion's count). For
                    # arity ≤ 2 predicates each action-edge has at most one
                    # shared position, so the intersection over a single set
                    # equals the previous per-position union — blocksworld /
                    # logistics behaviour is unchanged.
                    #
                    # NOTE (sys3+): for patterns larger than size 2, when
                    # extending a non-goal pattern fact, the candidate may
                    # legitimately be on the "effect" side and the pattern
                    # fact on the "precondition" side of an action-edge. We
                    # currently only handle the predecessor direction (goal
                    # as eff, candidate as pre), which is sufficient for sys2
                    # but would need a symmetric lookup for deeper BFS.
                    objs = atom.get_objects()
                    for shared in self._eff_pre_shared_positions.get(
                            (pred, co_pred), ()):
                        if not shared:
                            continue
                        # Build the constraint set for this action-edge: each
                        # eff_pos in `shared` fixes pre_pos=co_pred's position
                        # to the goal-atom's object at eff_pos. Then enumerate
                        # co_pred facts matching ALL these constraints (free
                        # positions iterate over the typed domain).
                        constraints = {
                            pre_pos: objs[eff_pos]
                            for eff_pos, pre_pos in shared.items()
                        }
                        for candidate in self._enumerate_matching(
                                co_pred, constraints):
                            if candidate in pattern_facts_set \
                                    or candidate in seen:
                                continue
                            candidates.append(candidate)
                            seen.add(candidate)
                else:
                    # Fallback: pred and co_pred co-occur in some action but
                    # share no schema variable. Two modes, selected by
                    # `self._bounded_fallback`:
                    #
                    # bounded (default) — if co_pred has any init-state atoms
                    #     use those; otherwise (effect-only predicate, true
                    #     orphan-effect-variable case) cap reachable
                    #     enumeration at _BOUNDED_FALLBACK_EFFECT_ONLY_CAP.
                    #     Avoids the rovers-1000-style explosion where
                    #     enumerating every reachable `at(?r, ?w)` adds 1000
                    #     candidates per goal singleton, while keeping the
                    #     effect-only orphan case (childsnack ontray, etc.)
                    #     well represented since those domains' reachable
                    #     atoms are far below the cap.
                    #
                    # unbounded — enumerate every reachable atom of co_pred,
                    #     matching pre-2026-05-26 behaviour. Used for
                    #     ablation studies via
                    #     `--pattern-gen-fallback-bound=off`.
                    if self._bounded_fallback:
                        init_facts = self._init_facts_by_pred.get(co_pred)
                        if init_facts:
                            source = init_facts
                        else:
                            source = self._enumerate_matching(co_pred, {})[
                                : self._BOUNDED_FALLBACK_EFFECT_ONLY_CAP
                            ]
                    else:
                        source = self._enumerate_matching(co_pred, {})
                    for candidate in source:
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
