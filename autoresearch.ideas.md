# Ideas backlog — projection-build optimizations

All must preserve projections exactly (checks compare h-value signatures). The
pinned config runs the NAIVE path (perf flags off), so target always-executed
code, not the already-gated optimizations.

## High-confidence, structural (try first)
1. **Hoist `MutableAction(projected_action)` + `make_sigma(...)` out of the
   per-src-state loop.** In `create_abstract_state_changing_transitions_v2` the
   `MutableAction` and `sigma0` are rebuilt for every (src_state × action) pair
   (line ~1111). They don't depend on src_state — build once per action before
   the src loop. Pure win, no semantic change.
2. **Hoist `param_domain_sizes_per_action.find(projected_action)`** out of the
   innermost emit callback (line ~1128) to once per action.
3. **Replace O(n²) linear `seen` dedup** (`std::any_of` + `EqualTo` over a
   `vector<SubstitutionFunction>`) with a hash set. Hot when an action emits many
   candidate substitutions per (src,dst).
4. **Replace linear `contains_atom`/`std::find`** in hot helpers with hashed
   membership: `difference_atoms` (O(n·m)), `collect_visible_fluent_atoms`,
   `verify_pattern_preconditions`'s pattern-atom scan, `check_one_negative`,
   `enumerate_fluent_pos_rec` ground-literal `contains_atom(src_atoms,...)`.
   Build a small hash set of src/dst atoms once per state.

## Medium
5. **Reduce `apply_substitution_fixpoint` allocations** — it builds new literal
   objects; in tight recursion this allocates. Consider reusing scratch buffers
   or computing groundness without materializing.
6. **Cache `collect_projected_static_atoms` / `collect_pattern_atoms`** across the
   state loop (already once per pattern via build_*; confirm not recomputed).
7. **Avoid rebuilding `src_atoms` vector** per state when states differ by one bit
   (incremental update across the 2^k state enumeration).
8. **`StaticAtomIndex` lookups**: ensure predicate lookup is hashed, not scanned.

## Speculative / larger
9. Order `projected_to_original_action` actions by selectivity so cheap actions
   prune first (but config pins declaration order — keep semantics identical;
   this only reorders work, allowed since it doesn't change results).
10. Skip actions whose effect predicates can't touch the pattern earlier/cheaper.
11. Pool/reserve vectors to cut per-call allocation churn (`transitions`,
    `adj_lists`, per-state temporaries).

## Off-loop validation
- After a few kept wins, run a hard excluded family (e.g. logistics s1000,
  organic prob01) once with a long timeout to confirm the speedup generalizes
  beyond the suite.
