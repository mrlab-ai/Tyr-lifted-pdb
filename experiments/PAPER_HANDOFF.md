# Paper handoff — algorithmic changes to write up

**Basis:** the kept-revisions grid experiment (all 17 revisions × full HTG suite, A\* +
canonical lifted-PDB). This compares each revision against the base on per-task
projection-build time, memory, and search behaviour. The point of this document is to
tell the write-up *which* changes are worth presenting as algorithmic contributions and
which are implementation noise. **Implementation details are deliberately omitted.**

## 1. What the data says is crucial

Per-revision step ratio (current/base of that step) of **projection-build time** and
**peak memory**, over all tasks that reached the build phase:

| change | projgen × | mem × | verdict |
|---|---|---|---|
| **self-loop subtree pruning** | **0.069** | 1.00 | **crucial (time)** |
| **share delete-relaxation / build datalog lazily** | 0.67 | **0.47** | **crucial (memory + time)** |
| static-relation hash indexing | 0.93 (tail) | 1.00 | minor on this suite; matters only on static-heavy domains |
| all other 13 revisions | 0.90–1.03 | 1.00 | implementation micro-opts — **omit from paper** |

Cumulative base → final: **projection build ×0.029 (−97 %)**, **peak memory ×0.47 (−53 %)**.

**The heuristic is unchanged.** Search time base → final on tasks solved by both is
**×1.003**, and the per-projection h-value signatures are identical. So every speedup is
a pure construction-cost reduction; none trades away heuristic quality. The paper should
state this explicitly — the optimisations make an otherwise-intractable construction
feasible *without* changing the abstraction or its heuristic values.

## 2. Algorithmic changes the paper must contain

### 2a. Self-loop subtree pruning (the central contribution)

When generating the abstract transitions of a projection, do **not** enumerate action
bindings that cannot change the abstract state. At every node of the
precondition/effect enumeration, apply an **anti-monotone feasibility test**: a binding
can change the projected state only if some visible ADD effect can still make true a
pattern atom that is absent in the source abstract state, or some DELETE effect can
remove a pattern atom present in it. If no visible effect can do so, the entire subtree
yields only **self-loops**, which are discarded at emission anyway — so pruning it is
**exact** (the resulting transition system, and hence the heuristic, is identical).

This changes the enumeration from *O(all action bindings)* to roughly *O(bindings that
touch the pattern)*, which is the difference between intractable and instant on the HTG
instances (−93 % in one step; the dominant effect in the −97 % cumulative result).

Write-up notes:
- Frame it as: the projection's transition enumeration is restricted to the
  **pattern-relevant** bindings. Bindings whose effects cannot alter any pattern atom
  are pruned at the earliest enumeration node at which infeasibility is provable.
- State the exactness argument (pruned subtrees produce only self-loops; self-loops are
  not abstract transitions), so admissibility/consistency are preserved.
- A worthwhile remark for the related-work/algorithm discussion: a *faithful effect-first*
  enumeration does **not** help under exact label semantics, because the skip/existential
  branches still require the full precondition join. Pruning the **self-loop class** is
  the correct formulation. (This is a non-obvious design point reviewers may ask about.)
- The incremental variant — re-checking feasibility only at binding sites that bind an
  effect-relevant parameter — is a constant-factor refinement of the same idea; it does
  not need separate treatment.

### 2b. Share per-task analysis across the pattern collection

The canonical heuristic builds one projection per pattern (tens per task). The
**delete-relaxation reachability** (used to filter unreachable atoms) and the task's
datalog programs depend only on the task, not the pattern, yet were recomputed for every
projection. Compute them **once per task and reuse them across all projections**
(equivalently: construct them lazily on first use). This is the memory result (−53 %
peak) and a third of the time result.

Write-up notes: present this as an algorithmic structuring point — *per-task* relaxed
reachability is shared by the whole pattern collection — not as a code refactor. It is
what makes the abstraction collection's memory scale with the task rather than with the
number of patterns.

### 2c. (Secondary) Static-relation indexing

Matching precondition literals against the static relations should use a hash index
keyed on the already-bound argument positions (turning an *O(|relation|)* scan per node
into *O(|matches|)*), and ground static-literal membership should be an *O(1)* hash test.
On the broad HTG suite this is near-neutral, but it is what keeps construction feasible
on static-relation-heavy domains (logistics/rovers/pipesworld-class). Mention it as an
enabler for large static relations; it is not central.

## 3. Scope limitation the paper must state

**Conditional effects are not supported.** The transition enumeration binds an effect's
variables through its *condition*; when a conditional effect targets a pattern atom but
its effect-local variables are constrained only by non-pattern condition atoms, the
transition is dropped, the abstract goal becomes unreachable, and the projection returns
h = ∞ for a solvable state (an inadmissible, unsound result). This was observed on
`genome-edit-distance-positional` (a solvable instance reported unsolvable). Until the
enumeration is extended to bind such effect-local variables from the pattern atoms
(effect-first enumeration for conditional effects), domains with conditional effects must
be **excluded from the abstraction’s scope** — state this as a limitation, and exclude
that domain class from the experiments rather than reporting its (wrong) results.

## 4. Experimental framing

- After these changes, **abstraction construction is no longer a bottleneck**:
  median ≈ 5 ms, max ≈ 122 ms over solved tasks; report it as negligible.
- The **remaining bottleneck is search** — predominantly memory (state storage), then
  time — not abstraction construction. If the paper has a "where time/memory goes"
  discussion, the construction phase should be shown as flat/negligible and the search
  phase as dominant.
- Because the heuristic is identical to the unoptimised construction, coverage/quality
  claims are about the *search using the heuristic*, not about the optimisations.

## 5. Omit from the paper

The other ~13 revisions are implementation-level (substitution representation and
allocation: shared/immutable substitution domains, trail-based in-place unification,
scratch-atom reuse, arithmetic parameter-slot lookup, linear vs hashed pattern-bit
lookup, hoisted per-action invariants, the O(n²)→hash-set duplicate-detection structure,
etc.). They contribute single-digit percentages each and carry no algorithmic content —
do not describe them in the paper.
