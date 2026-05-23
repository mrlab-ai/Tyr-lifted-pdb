#!/usr/bin/env bash
# Per-run measurement wrapper: pins to a CPU core and sets allocator/locale
# determinism. System-wide setup (CPU governor, turbo, ASLR, perf-counter
# noise suppression) belongs to `sudo pyperf system tune` — run that once
# per measurement session, not here.

set -euo pipefail

if [[ $# -eq 0 ]]; then
    echo "usage: $(basename "$0") <command> [args...]" >&2
    echo "  MEASURE_CORE=N  pin to core N (default: 2)" >&2
    exit 2
fi

: "${MEASURE_CORE:=2}"

command -v taskset >/dev/null 2>&1 \
    || { echo "measure.sh: taskset not found (install util-linux)" >&2; exit 1; }

echo "measure.sh: pinned to core ${MEASURE_CORE}; MALLOC_ARENA_MAX=1; LANG=C" >&2
exec taskset -c "${MEASURE_CORE}" env \
    MALLOC_ARENA_MAX=1 \
    LANG=C \
    "$@"
