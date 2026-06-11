# Autoresearch: lifted-PDB projection-build time

## Objective
Minimize the **projection-build time** of the lifted-PDB pipeline for the pinned
configuration, by optimizing the **implementation code** (C++ `tyr::core`, mainly
the projection generator). The flag configuration is FIXED — it is the
measurement harness, not a tuning knob.

## Metric
`projection_ms` = sum over the suite of `ProjectionGenerator.generate()` wall time
(ms), as printed by `autoresearch/bench_projection.py` (`PROJ_MS=`). **Lower is
better.** Per-task times are also logged (`METRIC <family>_ms=`).

Baseline (commit at branch start): **projection_ms ≈ 9240 ms**
(blocksworld 7377, visitall 1177, labyrinth 513, childsnack 115, genome 58).
NOTE: blocksworld is ~80% of the sum, so the headline metric is currently
dominated by it. Per-task lines reveal narrow wins; if the loop becomes
blocksworld-only, re-init a new segment using geometric mean of per-task times.

## Suite (one task per tractable HTG family, pinned config)
Defined in `autoresearch/suite.sh`. 5 families:
blocksworld, visitall(4-dim), labyrinth, childsnack, genome(positional).
Excluded — projection build > 35–120 s at this config, too slow to measure 3×:
logistics, rovers, organic-synthesis, pipesworld, genome(base/split).
These share the same scaling bottleneck (linear scans over huge object sets);
periodically spot-check one (e.g. logistics) OFF-loop to confirm wins generalize.

## How to run one experiment
1. **Edit** C++ in scope.
2. **Build:** `bash autoresearch/build.sh`  (incremental; non-zero exit = crash → revert).
3. **Measure:** `bash autoresearch.sh`  → parse the 3 `METRIC projection_ms=` lines.
4. **Decide:** `python3 ~/.claude/skills/autoresearch/scripts/decide.py --best "<best samples>" --candidate "<this run's samples>" --direction lower`
5. On KEEP candidate, **check correctness:** `bash autoresearch.checks.sh`
   (compares per-task h-value signatures vs `autoresearch/baseline_sig.txt`;
   exit≠0 = projections changed → revert as checks_failed).
6. KEEP → commit (`git add -A && git commit`), record `git rev-parse --short=7 HEAD`.
   DISCARD/CRASH/checks_failed → `git checkout -- .` (delete any new files by hand).
7. Append a result line to `autoresearch.jsonl`.

## Environment (see memory `tyr-build-and-run-env`)
- Toolchain: `source /tmp/tyr_env.sh` (GCC 13.3 via Lmod). Needed in build AND run shells.
- Build dir `build/` consumes prebuilt deps in `dependencies/installs`. Do NOT use
  `uv pip install .` (rebuilds boost/abseil from scratch).
- Run via `.venv/bin/python` with PYTHONPATH set (suite.sh `setup_env`).
- Bash tool shell is zsh: run loop scripts with `bash <script>` (no unquoted-var splitting).

## Files in scope (edit these)
- `src/planning/lifted_task/abstractions/projection_generator.cpp` (main hot path)
- `src/planning/lifted_task/abstractions/projection_generator/{task_projection,projection_join_plan}.{cpp,hpp}`
- `include/tyr/planning/lifted_task/abstractions/projection_generator.hpp`
- Supporting headers it calls into (unification, common/*) only if clearly safe.

## Off-limits (never edit to "win")
- The pinned flags / `autoresearch/suite.sh` `LPDB_FLAGS`.
- `python/prototypes/lifted_pdbs.py` (reference command).
- `autoresearch/bench_projection.py`, `autoresearch.sh`, `autoresearch.checks.sh`, `decide.py`.
- The PDDL benchmark tasks. The pattern set/count (changing it changes the metric meaning).

## Correctness invariant
Projections must be byte-for-byte equivalent in heuristic behaviour: the
canonical h0 and sorted per-projection h0 (the signature) must match the
baseline for every suite task. An "optimization" that changes any h-value is a
correctness regression, NOT a win (it usually means transitions were dropped).

## What's Been Tried
(none yet — baseline established)

## Insights / hot-path notes
- Config has projection perf-flags OFF (src_atoms_index, neg-pushdown, ineq-prop)
  and fluent order = declaration; reachability_filter ON. So the naive
  enumeration path is exercised. The obvious index optimization already exists
  behind `--projection-src-atoms-index on` but the flag is pinned off — so look
  for **always-executed** wins instead of re-implementing the gated ones.
- `create_abstract_state_changing_transitions_v2` is the core loop
  (per src abstract state × per projected action × enumerate bindings).
- Candidate hotspots (correctness-preserving): linear `contains_atom`/`std::find`
  over atom vectors; `MutableAction(projected_action)` + `make_sigma` rebuilt per
  (state,action) instead of per action; O(n²) linear `seen` dedup via `any_of`;
  `param_domain_sizes_per_action.find` inside the innermost emit callback.
  See `autoresearch.ideas.md`.
