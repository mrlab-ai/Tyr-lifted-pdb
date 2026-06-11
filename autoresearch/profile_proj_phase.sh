#!/bin/bash
# Profile ONLY the projection phase of one task: launch bench_projection (using
# the symbol-bearing build-prof), wait for the PATTERNS_DONE marker, then attach
# perf to the process for the remainder (R+ + projection build).
# Usage: bash autoresearch/profile_proj_phase.sh <taskfile>
set -uo pipefail
cd /proj/dfsplan/users/x_jense/lifted-pdbs
source /tmp/tyr_env.sh
export PYTHONPATH="$PWD/build-prof/python/src/pytyr:$PWD/python/prototypes:$PWD/python/src"
task="$1"; dom="$(dirname "$task")/domain.pddl"
FLAGS="--max-pattern-size 2 --max-pattern-count 50 --pattern-gen-backend cpp-systematic --pattern-gen-interesting on --pattern-gen-static-csp on --pattern-gen-reachability on --pattern-gen-fallback-bound on --pattern-gen-scorpion-match off --projection-fluent-literal-order declaration --projection-src-atoms-index off --projection-negative-literal-pushdown off --projection-inequality-propagation off --projection-reachability-filter on --cost-type one"
OUT=/tmp/tyr_projphase_run.log
DATA=/tmp/tyr_projphase.data
: > "$OUT"
.venv/bin/python autoresearch/bench_projection.py -d "$dom" -p "$task" $FLAGS > "$OUT" 2>&1 &
PID=$!
# Wait for the marker, then attach perf until the process exits.
while ! grep -q "PATTERNS_DONE" "$OUT" 2>/dev/null; do
  kill -0 "$PID" 2>/dev/null || { echo "process died early"; cat "$OUT"; exit 1; }
  sleep 0.5
done
perf record -g --call-graph dwarf -o "$DATA" -p "$PID" -- sleep 100000 &
PERF=$!
wait "$PID"
kill -INT "$PERF" 2>/dev/null; wait "$PERF" 2>/dev/null
grep -E "PROJ_MS|NPROJ" "$OUT"
echo "=== flat self-time (projection phase only) ==="
perf report -i "$DATA" --stdio -g none --no-children --percent-limit 1.5 2>/dev/null \
  | grep -vE "^#" | awk '{ s=$0; if (length(s)>170) s=substr(s,1,170); print s }' | head -25
