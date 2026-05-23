#!/usr/bin/env bash
# Compare a fresh projection_generator JSON against a recorded baseline.
# Wraps Google Benchmark's tools/compare.py, which lives inside the vendored
# benchmark dep after the C++ deps build.

set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "usage: $(basename "$0") <baseline.json> <new.json> [extra args to compare.py...]" >&2
    exit 2
fi

baseline="$1"
new="$2"
shift 2

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ -n "${GBENCH_COMPARE:-}" ]]; then
    compare="${GBENCH_COMPARE}"
else
    compare="${repo_root}/dependencies/build/benchmark/src/benchmark/tools/compare.py"
fi

if [[ -n "${PYTHON:-}" ]]; then
    py="${PYTHON}"
elif [[ -x "${repo_root}/.venv/bin/python" ]]; then
    py="${repo_root}/.venv/bin/python"
else
    py="python3"
fi

if [[ ! -f "${compare}" ]]; then
    cat >&2 <<EOF
compare.sh: compare.py not found at ${compare}

Either build the vendored deps so it's available:
  cmake -S dependencies -B dependencies/build \\
      -DCMAKE_INSTALL_PREFIX=\$PWD/dependencies/installs
  cmake --build dependencies/build -j4

Or point at an existing copy:
  GBENCH_COMPARE=/path/to/compare.py compare.sh ...
EOF
    exit 1
fi

"${py}" -c "import numpy, scipy" 2>/dev/null \
    || { echo "compare.sh: ${py} needs numpy and scipy (pip install numpy scipy, or set PYTHON to an interpreter that has them)" >&2; exit 1; }

exec "${py}" "${compare}" benchmarks "${baseline}" "${new}" "$@"
