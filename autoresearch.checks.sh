#!/bin/bash
# Correctness oracle (run AFTER a passing benchmark, off the metric clock).
#
# The projection optimizations must NOT change the resulting abstraction. We
# verify this by comparing a per-task correctness signature — the canonical
# heuristic's initial-state h-value plus the sorted per-projection initial
# h-values — against the recorded baseline in autoresearch/baseline_sig.txt.
# Identical projections => identical h-values => identical signature.
#
# Exit 0 = all signatures match (correctness preserved); exit 1 = a mismatch
# (revert). Pass --record to (re)generate the baseline file instead of checking.
set -uo pipefail
cd /proj/dfsplan/users/x_jense/lifted-pdbs
source autoresearch/suite.sh
setup_env

SIGFILE=autoresearch/baseline_sig.txt
MODE="${1:-check}"

compute_sig() {  # echoes "label SIG=..."
  local label="$1"; local task="$2"; local dom; dom="$(dirname "$task")/domain.pddl"
  local out; out=$(timeout 300 $PY autoresearch/bench_projection.py -d "$dom" -p "$task" $LPDB_FLAGS --sig 2>&1)
  local sig; sig=$(echo "$out" | grep -oP 'SIG=\K.*')
  echo "$label ${sig:-MISSING}"
}

if [ "$MODE" = "--record" ]; then
  : > "$SIGFILE"
  for entry in "${SUITE[@]}"; do
    label="${entry%%;*}"; task="${entry#*;}"
    line=$(compute_sig "$label" "$task")
    echo "$line" | tee -a "$SIGFILE"
  done
  echo "RECORDED -> $SIGFILE"
  exit 0
fi

if [ ! -f "$SIGFILE" ]; then
  echo "CHECKS_ERROR: no baseline signature file ($SIGFILE); run with --record first" >&2
  exit 2
fi

fail=0
for entry in "${SUITE[@]}"; do
  label="${entry%%;*}"; task="${entry#*;}"
  cur=$(compute_sig "$label" "$task")
  exp=$(grep "^$label " "$SIGFILE" || echo "")
  if [ "$cur" != "$exp" ]; then
    echo "CHECK_FAIL $label"
    echo "  expected: $exp"
    echo "  actual:   $cur"
    fail=1
  else
    echo "CHECK_OK $label"
  fi
done
[ "$fail" -eq 0 ] && echo "CHECKS_PASSED" || echo "CHECKS_FAILED"
exit "$fail"
