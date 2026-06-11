#!/bin/bash
# Calibration: measure projection-build time for the smallest instance of each
# HTG family, with the pinned configuration. Helps pick one task per family at a
# hardness where projection build is measurable but a rep stays fast.
set -uo pipefail
cd /proj/dfsplan/users/x_jense/lifted-pdbs
source /tmp/tyr_env.sh
export PYTHONPATH="$PWD/build/python/src/pytyr:$PWD/python/prototypes:$PWD/python/src"
HTG=/tmp/htg-domains
PY=.venv/bin/python
FLAGS="--max-pattern-size 2 --max-pattern-count 50 --pattern-gen-backend cpp-systematic --pattern-gen-interesting on --pattern-gen-static-csp on --pattern-gen-reachability on --pattern-gen-fallback-bound on --pattern-gen-scorpion-match off --projection-fluent-literal-order declaration --projection-src-atoms-index off --projection-negative-literal-pushdown off --projection-inequality-propagation off --projection-reachability-filter on --cost-type one"

run() {
  local label="$1"; local task="$2"
  local dom; dom="$(dirname "$task")/domain.pddl"
  local out; out=$(timeout 120 $PY autoresearch/bench_projection.py -d "$dom" -p "$task" $FLAGS 2>&1)
  local ms; ms=$(echo "$out" | grep -oP 'PROJ_MS=\K[0-9.]+')
  local npat; npat=$(echo "$out" | grep -oP 'NPAT=\K[0-9]+')
  printf '%-14s PROJ_MS=%-12s NPAT=%-4s %s\n' "$label" "${ms:-TIMEOUT/ERR}" "${npat:-?}" "$(basename "$task")"
}

run blocksworld  "$HTG/blocksworld-large-simple/goal-2/p-100-2.pddl"
run childsnack   "$HTG/childsnack-contents/parsize1-cham3/contentam1-p0.pddl"
run genome-ed    "$HTG/genome-edit-distance/d-1-2.pddl"
run labyrinth    "$HTG/labyrinth/OPT/p01.pddl"
run logistics    "$HTG/logistics-large-simple/goal-1/p-a1-c1-s1000-p10-t1-g1.pddl"
run organic-syn  "$HTG/organic-synthesis-original/prob01.pddl"
run pipesworld   "$HTG/pipesworld-tankage-nosplit/p01-net1-b6-g2-t50.pddl"
run rovers       "$HTG/rovers-large-simple/goal-2/p-r1-w1000-o1-1-g2.pddl"
run visitall     "$HTG/visitall-multidimensional/3-dim-visitall-CLOSE-g1/p0.pddl"
echo "CALIBRATION_DONE"
