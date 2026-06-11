#!/bin/bash
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
run childsnack-p1c5     "$HTG/childsnack-contents/parsize1-cham5/contentam1-p0.pddl"
run childsnack-p2c3     "$HTG/childsnack-contents/parsize2-cham3/contentam2-p0.pddl"
run childsnack-p2c5     "$HTG/childsnack-contents/parsize2-cham5/contentam2-p0.pddl"
run organic-orig-p2     "$HTG/organic-synthesis-original/prob02.pddl"
run organic-alkene-p1   "$HTG/organic-synthesis-alkene/p1.pddl"
run organic-alkene-p2   "$HTG/organic-synthesis-alkene/p2.pddl"
run organic-MIT-p2      "$HTG/organic-synthesis-MIT/p2.pddl"
run logistics-probx1    "$HTG/logistics/prob-x1.pddl"
run pipes-p01           "$HTG/pipesworld-tankage-nosplit/p01-net1-b6-g2-t50.pddl"
echo "PROBE2_DONE"
