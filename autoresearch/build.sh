#!/bin/bash
# Incremental rebuild of the pytyr extension after a C++ edit. This is the "edit"
# step of the loop. Exits non-zero on compile/link failure (treated as a crash).
set -uo pipefail
cd /proj/dfsplan/users/x_jense/lifted-pdbs
source /tmp/tyr_env.sh
cmake --build build --target pytyr -j"$(nproc)" 2>&1 | tail -25
status=${PIPESTATUS[0]}
if [ "$status" -ne 0 ]; then
  echo "BUILD_FAILED status=$status"
  exit "$status"
fi
echo "BUILD_OK"
