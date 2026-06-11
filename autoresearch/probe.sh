#!/bin/bash
# Probe candidate instances to assemble a balanced suite (target per-task
# projection build ~0.1-10s). Short per-probe timeout to fail fast.
set -uo pipefail
cd /proj/dfsplan/users/x_jense/lifted-pdbs
source /tmp/tyr_env.sh
export PYTHONPATH="$PWD/build/python/src/pytyr:$PWD/python/prototypes:$PWD/python/src"
HTG=/tmp/htg-domains
PY=.venv/bin/python
FLAGS="--max-pattern-size 2 --max-pattern-count 50 --pattern-gen-backend cpp-systematic --pattern-gen-interesting on --pattern-gen-static-csp on --pattern-gen-reachability on --pattern-gen-fallback-bound on --pattern-gen-scorpion-match off --projection-fluent-literal-order declaration --projection-src-atoms-index off --projection-negative-literal-pushdown off --projection-inequality-propagation off --projection-reachability-filter on --cost-type one"
TMO=${TMO:-35}

run() {
  local label="$1"; local task="$2"
  [ -f "$task" ] || { printf '%-26s MISSING %s\n' "$label" "$task"; return; }
  local dom; dom="$(dirname "$task")/domain.pddl"
  local out; out=$(timeout "$TMO" $PY autoresearch/bench_projection.py -d "$dom" -p "$task" $FLAGS 2>&1)
  local ms; ms=$(echo "$out" | grep -oP 'PROJ_MS=\K[0-9.]+')
  local npat; npat=$(echo "$out" | grep -oP 'NPAT=\K[0-9]+')
  printf '%-26s PROJ_MS=%-12s NPAT=%-4s %s\n' "$label" "${ms:-TIMEOUT}" "${npat:-?}" "$(basename "$task")"
}

# bigger versions of fast families
run childsnack-p2c5    "$HTG/childsnack-contents/parsize2-cham5/contentam1-p0.pddl"
run childsnack-p3c7    "$HTG/childsnack-contents/parsize3-cham7/contentam1-p0.pddl"
run visitall-4dim      "$HTG/visitall-multidimensional/4-dim-visitall-CLOSE-g1/p0.pddl"
run visitall-5dim      "$HTG/visitall-multidimensional/5-dim-visitall-CLOSE-g1/p0.pddl"
# slow families: try variants / smaller
run logistics-base1    "$HTG/logistics/p01.pddl"
run genome-split       "$HTG/genome-edit-distance-split/d-1-2.pddl"
run genome-positional  "$HTG/genome-edit-distance-positional/d-1-2.pddl"
run organic-alkene     "$HTG/organic-synthesis-alkene/prob01.pddl"
run organic-MIT        "$HTG/organic-synthesis-MIT/prob01.pddl"
echo "PROBE_DONE"
