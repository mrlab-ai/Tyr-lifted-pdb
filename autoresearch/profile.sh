#!/bin/bash
# Profile one suite task's projection build (RelWithDebInfo build-prof) and print
# a flat self-time hotspot list. Usage: bash autoresearch/profile.sh <taskfile>
set -uo pipefail
cd /proj/dfsplan/users/x_jense/lifted-pdbs
source /tmp/tyr_env.sh
export PYTHONPATH="$PWD/build-prof/python/src/pytyr:$PWD/python/prototypes:$PWD/python/src"
task="$1"; dom="$(dirname "$task")/domain.pddl"
FLAGS="--max-pattern-size 2 --max-pattern-count 50 --pattern-gen-backend cpp-systematic --pattern-gen-interesting on --pattern-gen-static-csp on --pattern-gen-reachability on --pattern-gen-fallback-bound on --pattern-gen-scorpion-match off --projection-fluent-literal-order declaration --projection-src-atoms-index off --projection-negative-literal-pushdown off --projection-inequality-propagation off --projection-reachability-filter on --cost-type one"
DATA=/tmp/tyr_prof_$(basename "$task").data
perf record -g -o "$DATA" --call-graph dwarf -- \
  .venv/bin/python autoresearch/bench_projection.py -d "$dom" -p "$task" $FLAGS > /tmp/tyr_prof_run.log 2>&1
grep PROJ_MS /tmp/tyr_prof_run.log
echo "=== flat self-time (top, demangled, truncated) ==="
perf report -i "$DATA" --stdio -g none --no-children --percent-limit 1.5 2>/dev/null \
  | grep -vE "^#" | awk '{ s=$0; if (length(s)>180) s=substr(s,1,180); print s }' | head -25
