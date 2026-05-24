# Profiling Tyr

Two profiling tracks, for two different questions:

- **Wall-time microbenchmarks** — *did this change make Tyr faster or slower?* Run via [`profiling/runner.py`](../profiling/runner.py), which drives per-target Google Benchmark binaries built with `BUILD_PROFILING=ON`.
- **Function-level call profiling** — *where is time going?* Run `gbfs_lazy` or `astar_eager` (`BUILD_EXECUTABLES=ON`) under `valgrind --tool=callgrind` and inspect with `kcachegrind` or `callgrind_annotate`.

A methodology layer for trustworthy, comparable wall-time numbers is built into the runner itself — per-case wall-clock timeouts, summary JSONs capturing git/build/host context, and a typed schema for attribute comparison. See [`profiling/README.md`](../profiling/README.md) for the canonical reference.

## Wall-time: profiling/runner.py

Per-target binaries live under `profiling/planning/...`. The runner spawns one Google Benchmark subprocess per case with a hard wall-clock timeout, captures git/build/host metadata in a `summary.json`, and writes per-case result JSONs only after a successful subprocess. Failed and timed-out cases are recorded separately with their failure reason.

Build and run a full sweep:

```bash
cmake --build build --target projection_generator -j4
./profiling/runner.py \
    --executable build/profiling/planning/lifted_task/abstractions/projection_generator \
    --output-dir profiling-results/planning/lifted_task/abstractions/projection_generator \
    --suite-json profiling/planning/lifted_task/abstractions/projection_generator.json \
    --benchmark-min-time 0.1s \
    --benchmark-timeout 60 \
    --benchmark-repetitions 5 \
    --benchmark-report-aggregates-only
```

Compare two completed runs:

```bash
./profiling/compare.py \
    old-results/planning/lifted_task/abstractions/projection_generator/summary.json \
    profiling-results/planning/lifted_task/abstractions/projection_generator/summary.json \
    --output profiling-results/planning/lifted_task/abstractions/projection_generator/compare.json
```

When debugging a single case or registration, invoke Google Benchmark directly without the runner:

```bash
./build/profiling/planning/lifted_task/abstractions/projection_generator --benchmark_list_tests=true
./build/profiling/planning/lifted_task/abstractions/projection_generator \
    --benchmark_filter='^blocksworld-large-simple/profiling-1/.*'
```

If the binary prints `***WARNING*** CPU scaling is enabled` on startup, treat the numbers as soft. For trustworthy comparisons, prime the system once per measurement session with `sudo pyperf system tune` (sets governor=performance, disables turbo + ASLR, suppresses perf-counter noise; reverse with `sudo pyperf system reset`).

## Function profiling: callgrind on a planner

`gbfs_lazy` and `astar_eager` are the standard entry points. The `-N 1` flag expands at most one node, which is enough to capture the hot path of successor generation while keeping the trace bounded.

Whole-program profile:

```bash
valgrind \
    --tool=callgrind \
    --collect-jumps=yes \
    --dump-instr=yes \
    ./build/exe/gbfs_lazy \
        -D data/profiling/htg/rovers-large-simple/domain.pddl \
        -P data/profiling/htg/rovers-large-simple/p-r1-w1000-o1-1-g2.pddl \
        -O plan.txt \
        -N 1
```

Focused profile of a specific hot function (here, the Datalog k-PKC delta loop):

```bash
valgrind \
    --tool=callgrind \
    --callgrind-out-file=callgrind.out \
    --toggle-collect='_ZN3tyr7datalog4kpkc9DeltaKPKC24set_next_assignment_setsERKNS0_22StaticConsistencyGraphERKNS0_14AssignmentSetsE' \
    --collect-jumps=yes \
    --dump-instr=yes \
    ./build/exe/gbfs_lazy \
        -D data/profiling/htg/rovers-large-simple/domain.pddl \
        -P data/profiling/htg/rovers-large-simple/p-r1-w1000-o1-1-g2.pddl \
        -O plan.txt \
        -N 1
```

`--toggle-collect` takes a mangled C++ symbol. To find one, grep the binary's symbol table for the function name — Itanium ABI mangling preserves the identifier as a substring:

```bash
nm build/exe/gbfs_lazy | grep set_next_assignment_sets
```
