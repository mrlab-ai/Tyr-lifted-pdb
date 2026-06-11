#!/bin/bash
# autoresearch metric runner: measures total projection-build time (ms) across
# the suite. Prints one `METRIC projection_ms=<sum>` line per repetition (plus
# per-task METRIC lines for visibility). Does NOT build — building is the edit
# step (autoresearch/build.sh). See autoresearch.md.
set -uo pipefail
cd /proj/dfsplan/users/x_jense/lifted-pdbs
source autoresearch/suite.sh
setup_env

REPS=${REPS:-3}
WARMUP=${WARMUP:-1}

# --- sanity pre-check (<1s): import must work ---
if ! $PY -c "import pytyr" 2>/dev/null; then
  echo "SANITY_FAIL: pytyr import failed" >&2
  exit 3
fi

run_task() {  # echoes PROJ_MS for one task, or "ERR"
  local task="$1"; local dom; dom="$(dirname "$task")/domain.pddl"
  local out; out=$(timeout 120 $PIN $PY autoresearch/bench_projection.py -d "$dom" -p "$task" $LPDB_FLAGS 2>&1)
  echo "$out" | grep -oP 'PROJ_MS=\K[0-9.]+' || echo "ERR"
}

run_suite_once() {  # echoes the summed projection_ms, prints per-task lines
  local total=0
  for entry in "${SUITE[@]}"; do
    local label="${entry%%;*}"; local task="${entry#*;}"
    local ms; ms=$(run_task "$task")
    if [ "$ms" = "ERR" ]; then echo "TASK_ERR $label" >&2; return 1; fi
    printf 'METRIC %s_ms=%s\n' "$label" "$ms"
    total=$(awk -v a="$total" -v b="$ms" 'BEGIN{printf "%.4f", a+b}')
  done
  printf 'METRIC projection_ms=%s\n' "$total"
}

# --- warmup (discarded) ---
for ((w=0; w<WARMUP; w++)); do run_suite_once >/dev/null 2>&1 || { echo "WARMUP_FAIL" >&2; exit 1; }; done

# --- timed repetitions ---
for ((r=0; r<REPS; r++)); do
  run_suite_once || exit 1
done
