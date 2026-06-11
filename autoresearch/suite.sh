#!/bin/bash
# Shared definitions for the autoresearch loop: environment, pinned config flags,
# and the benchmark suite (one task per tractable HTG domain family).
#
# Metric: projection-build time (ms), measured by autoresearch/bench_projection.py
# which mirrors the pinned `lifted_pdbs.py` configuration exactly but times only
# ProjectionGenerator.generate().
#
# The pinned configuration (DO NOT CHANGE — it is the fixed measurement harness;
# the loop optimizes the C++/Python implementation, not these flags):
LPDB_FLAGS="--max-pattern-size 2 --max-pattern-count 50 \
--pattern-gen-backend cpp-systematic --pattern-gen-interesting on \
--pattern-gen-static-csp on --pattern-gen-reachability on \
--pattern-gen-fallback-bound on --pattern-gen-scorpion-match off \
--projection-fluent-literal-order declaration --projection-src-atoms-index off \
--projection-negative-literal-pushdown off --projection-inequality-propagation off \
--projection-reachability-filter on --cost-type one"

HTG=/tmp/htg-domains
REPO=/proj/dfsplan/users/x_jense/lifted-pdbs
PY="$REPO/.venv/bin/python"

# Suite: "label;taskfile"  (domain is the sibling domain.pddl)
# One representative per tractable family at suitable hardness (projection build
# ~0.07-7.6s at the pinned config). logistics/rovers/organic/pipesworld are
# excluded: their smallest HTG instances exceed 35-120s projection build at this
# config and cannot be measured 3x per loop iteration.
SUITE=(
  "blocksworld;$HTG/blocksworld-large-simple/goal-2/p-100-2.pddl"
  "visitall;$HTG/visitall-multidimensional/4-dim-visitall-CLOSE-g1/p0.pddl"
  "labyrinth;$HTG/labyrinth/OPT/p01.pddl"
  "childsnack;$HTG/childsnack-contents/parsize2-cham5/contentam2-p0.pddl"
  "genome;$HTG/genome-edit-distance-positional/d-1-2.pddl"
)

setup_env() {
  source /tmp/tyr_env.sh
  export PYTHONPATH="$REPO/build/python/src/pytyr:$REPO/python/prototypes:$REPO/python/src"
  export MALLOC_ARENA_MAX=1
}

# Pin to a single idle core for measurement stability (projection build is
# single-threaded: ExecutionContext(1)).
PIN="taskset -c 3"
