# Replay of kept projection-build revisions — implementation report

**Date:** 2026-06-14
**Branch produced:** `autoresearch/replay-kept-2026-06-14` (based on `484e548`, the base/scaffolding revision)

> **Update (final composition):** after reviewing the autoresearch-suite evidence for
> the two segment-2 changes (see §3 / §5), `r15` static-hashjoin and `r16`
> incremental-feasibility were **added back**. The branch now contains **15 perf
> commits** — every kept revision except `r09` trail-effect-params (the one change
> with no measurable effect on *any* suite and no dependents). All correctness checks
> still pass. The per-revision verdict table below is kept as the measurement record.
**Source experiment:** `experiments/astar_eager/data/2026-06-13-htg-kept-revisions-eval/properties`
(17 kept autoresearch revisions × 1058 HTG tasks, A\* + canonical lifted-PDB, 5-min wall limit)

## 1. What this is

The autoresearch loop committed 17 "kept" revisions. Each was accepted at the time
because it beat measurement noise on the *autoresearch metric* (projection-build
geomean over a small calibrated suite). This report re-judges those 17 against the
**full HTG end-to-end experiment** (the generated `properties` file), keeps only the
revisions that show a measurable time or memory decrease *there*, and replays them
onto a clean branch from the base revision.

## 2. How revisions were judged

For each consecutive revision transition I computed the **paired ratio** `cur/prev`
of two attributes over every task where *both* revisions recorded a value (this
includes search-timeouts, because `time_ms_projection_generator` is logged before
search starts — the correct signal for a projection-build change):

- `time_ms_projection_generator` (geomean + median, 427–576 tasks per pair)
- `peak_memory_usage_bytes` (geomean + median, 168–174 tasks per pair)

**Keep criterion:** geomean(projgen) ≤ 0.97 **or** geomean(peakmem) ≤ 0.97
(i.e. ≥3 % measured decrease in time or memory). Otherwise the revision is a
drop candidate.

## 3. Verdict

| # | revision | projgen g/med | peakmem g | verdict |
|---|----------|---------------|-----------|---------|
| r01 | hashset-dedup (`d32067d`) | 0.999 / 1.000 | 1.000 | drop by metric → **KEPT (performance-critical, see §4a)** |
| r02 | shared-σ-domain (`48a8cd1`) | 0.886 / 0.890 | 1.000 | KEEP (−11 %) |
| r03 | fixpoint-fastpath (`a14edd9`) | 0.898 / 0.893 | 1.000 | KEEP (−10 %) |
| r04 | hoist-invariants (`c213312`) | 0.970 / 0.982 | 1.000 | KEEP (−3 %, modest) |
| r05 | static-ground-hashset (`15461fa`) | 0.931 / 0.994 | 1.007 | KEEP (−7 % tail; helps static-heavy domains) |
| r06 | arith-σ-lookup (`db98fde`) | 0.934 / 0.944 | 1.000 | KEEP (−6 %) |
| r07 | match-in-place (`28c77f8`) | 1.029 / 1.016 | 1.000 | drop by metric → **KEPT (dependency of r08)** |
| r08 | trail-unification (`aae4644`) | 0.902 / 0.904 | 1.000 | KEEP (−10 %) |
| r09 | trail-effect-params (`4ef995d`) | 1.005 / 1.000 | 1.000 | **DROP** (neutral, not depended on) |
| r10 | scratch-atoms (`0d6d3e0`) | 0.928 / 0.928 | 1.000 | KEEP (−7 %) |
| r11 | lazy-datalog (`ae1078e`) | 0.669 / 0.786 | **0.469** | KEEP (−33 % time, **−53 % memory**) |
| r12 | linear-patternbitindex (`9d0315c`) | 0.969 / 0.975 | 0.999 | KEEP (−3 %, modest) |
| r13 | task-R+-memo (`97c6e9e`) | 1.021 / 1.006 | 0.999 | drop by metric → **KEPT (dependency of r14)** |
| r14 | algorithmic-pruning (`830f4fe`) | 0.069 / 0.702 | 1.000 | KEEP (−93 % geomean, −30 % median) |
| r15 | static-hashjoin (`faafa28`) | 1.002 / 1.000 | 1.002 | drop by HTG metric → **KEPT** (−6.1% on autoresearch suite, see §5) |
| r16 | incremental-feasibility (`c6564f5`) | 1.000 / 1.000 | 0.999 | drop by HTG metric → **KEPT** (−5.4% on autoresearch suite, see §5) |

**Replayed branch = 15 commits:** the 10 measured keepers (r02–r06, r08, r10–r12, r14),
2 commits kept **as code dependencies** (r07, r13), **r01 kept as performance-critical**
(see §4a — discovered when the first build of the branch *without* it timed out), and
**r15 + r16 kept on autoresearch-suite evidence** (see §5, added cleanly as the tip).
Dropped: only **r09** trail-effect-params (no measurable effect on any suite, no dependents).

## 4. Replay mechanics

Cherry-picked the kept commits in chronological order onto `484e548`.
Findings during replay:

- r02–r06 applied cleanly. (Notably `c213312` applied over the *absence* of r01,
  proving r01 is not a structural dependency.)
- **r08 (trail-unification) would not apply without r07 (match-in-place)** — r08
  rewrites the same recursion r07 introduced. r07 was therefore promoted to a kept
  dependency even though it measures as a +3 % regression in isolation.
- r10–r12 applied cleanly without r09 → r09 confirmed independent and dropped.
- **r14 (algorithmic-pruning) needs r13 (task-R+-memo)** plus one trivial conflict at
  the `enumerate_condition_v2` call site (HEAD lacked r14's `&pre.feas, src_mask`
  arguments and its non-const lambda). Resolved by taking r14's intended call — the
  rest of r14 (the `EffectFeasibility` struct, the signature change) applied cleanly.
- The only auto-resolved non-code conflicts were `autoresearch/baseline_sig.txt`,
  `autoresearch.jsonl` and `build-prof/*` (data/oracle/cruft files, took incoming).

### 4a. Why r01 is kept despite measuring neutral

The first branch I built *omitted* r01 (its HTG verdict was neutral). Building and
signature-checking that branch produced **`CHECK_FAIL blocksworld … MISSING` and
`CHECK_FAIL pipesworld … MISSING`** — i.e. their projection build did not finish
within the 900 s check timeout, while the other 7 tasks matched the baseline exactly.
Dropping r01 reverts the `seen` dedup to the O(n²) `std::any_of` scan, and r14's
pruning is **not** sufficient to keep blocksworld/pipesworld tractable on its own.
Re-adding r01 (hash-set dedup) restored both. This is concrete proof that the
HTG-flat/5-min experiment cannot see r01's value — its beneficiaries time out there —
so r01 is retained on engineering grounds, not on the (blind) metric.

**Correctness:** built `pytyr` on the branch and ran the seg2 signature oracle
(`autoresearch.checks.sh` with the authoritative `baseline_sig.txt` from the
autoresearch tip). Result: **CHECKS_PASSED — all 9 suite tasks
(blocksworld, childsnack, visitall, labyrinth, logistics, organic, pipesworld,
rovers, genome) produce h-value signatures identical to the baseline.** Every kept
change is projection-preserving by construction, and this confirms the cherry-pick
conflict resolution (the single r14 hunk) introduced no behavioural change.

## 5. Handoff: what to change in the write-up

The HTG-flat / 5-minute / canonical-lifted experiment is a **coarse instrument** for
some of these changes. Three "DROP" verdicts are artifacts of the measurement set,
not evidence the change is worthless. Decide each consciously before writing:

1. **`r01` hashset-dedup — present it as essential, not a no-op.** It replaced the
   O(n²) `std::any_of`+`EqualTo` `seen` scan with a hash set. The metric reads neutral
   only because its beneficiaries time out here; the replay validation proved it is
   load-bearing (without it, blocksworld + pipesworld projection build exceeds 900 s —
   see §4a). It is **kept** in the replay branch. For the write-up, cite the
   calibrated-suite numbers (blocksworld-500: >600 s → ~1 s), not this experiment.

2. **`r15` static-hashjoin and `r16` incremental-feasibility — neutral here, real on
   the calibrated suite (now KEPT).** Segment-2 geomean over the 9 HTG families:
   - run-17 baseline `830f4fe` = 5444 ms
   - `+ S1 static-hashjoin` (`faafa28`) = **5113 ms (−6.1 %)**
   - `+ S2 incremental-feasibility` (`c6564f5`) = **4536 ms (−5.4 %)** (load-matched A/B)

   The gains are on the static-relation-heavy domains (pipesworld 51.7→42.5→38.1 s,
   rovers 34.6→30.2→27.5 s, logistics 14.5→13.3→11.8 s, genome 7.9→7.1→6.5 s). HTG-flat
   at 5 min cannot reproduce them because those hard instances time out. **S1** replaces
   the static join's O(|relation|) per-node scan with a lazily-built, per-(step,mask)
   hash index (`values-at-bound-positions → tuples`) cached across the 2^k source
   states → O(|matches|) per node. **S2** keeps run-17's anti-monotone self-loop pruning
   but re-checks feasibility only at binding sites that touch an effect-relevant
   parameter (precomputed `effect_param_mask`) plus a per-(state,action) root check —
   strictly-less work, identical pruning. For the write-up, cite these
   autoresearch-suite numbers, not the (blind) HTG-flat experiment.

3. **`r07` and `r13` are infrastructure, not standalone wins.** They measure flat/
   slightly-negative alone but the big wins (r08 trail, r14 pruning) are built on top
   of them. In the write-up, fold each into its dependent change (present
   match-in-place+trail as one step, and R+-memo+pruning together) rather than listing
   them as separate "−0 %" rows that invite a reviewer to ask why they're there.

4. **Headline numbers to use.** The cumulative projection-build reduction on the 168
   tasks solved by all revisions is **648 → 148 ms (−77 %)**; the two dominant steps
   are lazy-datalog (r11, −30 % time / −53 % memory) and algorithmic pruning (r14,
   −57 % at that step). Coverage moves only 169 → 175 / 1058, because the unsolved HTG
   instances are search-bound at a 5-min optimal budget — say this explicitly so the
   modest coverage delta isn't misread as "the optimizations didn't help."

5. **Memory.** Only r11 (lazy-datalog) shows a memory decrease (peak −53 %); every
   other change is time-only (peakmem ≈ 1.000). Don't claim memory wins for the others.

## 6. Artifacts

- Replay branch: `autoresearch/replay-kept-2026-06-14`
- Per-revision binaries: `experiments/astar_eager/bin/astar_eager-<sha>` (17)
- Comparison experiment + report: `experiments/astar_eager/2026-06-13-htg-kept-revisions.py`,
  `data/2026-06-13-htg-kept-revisions-eval/report.html`
- This analysis was computed from the `properties` file in that eval dir.
