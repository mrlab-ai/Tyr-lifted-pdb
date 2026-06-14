#!/bin/bash
# Build the astar_eager planner at each kept autoresearch revision, in
# chronological order, in a dedicated detached worktree so the main tree's
# autoresearch branch state is untouched. Consecutive commits differ by a few
# files, so each build after the first is incremental (fast).
#
# Binaries are saved to experiments/astar_eager/bin/astar_eager-<shortsha>.
set -uo pipefail

REPO=/proj/dfsplan/users/x_jense/lifted-pdbs
WT=/proj/dfsplan/users/x_jense/lpdb-buildwt
DEPS=$REPO/dependencies/installs
BIN=$REPO/experiments/astar_eager/bin
source /tmp/tyr_env.sh 2>/dev/null

# Chronological order (oldest -> newest). 484e548 is built separately (full).
COMMITS=(484e548 d32067d 48a8cd1 a14edd9 c213312 15461fa db98fde 28c77f8 \
         aae4644 4ef995d 0d6d3e0 ae1078e 9d0315c 97c6e9e 830f4fe faafa28 c6564f5)

mkdir -p "$BIN"
for c in "${COMMITS[@]}"; do
  out="$BIN/astar_eager-$c"
  if [ -s "$out" ]; then echo "SKIP $c (already built)"; continue; fi
  echo "=== checkout $c ==="
  git -C "$WT" checkout --detach "$c" 2>&1 | tail -1
  start=$(date +%s)
  cmake --build "$WT/build" --target astar_eager -j32 > "/tmp/build-$c.log" 2>&1
  st=$?
  if [ "$st" -ne 0 ] || [ ! -s "$WT/build/exe/astar_eager" ]; then
    echo "BUILD_FAIL $c (status $st) -- see /tmp/build-$c.log"; tail -5 "/tmp/build-$c.log"; continue
  fi
  cp "$WT/build/exe/astar_eager" "$out"
  echo "OK $c  $(( $(date +%s) - start ))s  $(du -h "$out" | cut -f1)"
done
echo "ALL_BUILDS_DONE"
ls -la "$BIN"