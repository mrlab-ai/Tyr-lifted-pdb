# Profiling Tyr

Two profiling tracks, for two different questions:

- **Wall-time microbenchmarks** — *did this change make Tyr faster or slower?* Run via `profiling/projection_generator`, a Google Benchmark binary built with `BUILD_PROFILING=ON`.
- **Function-level call profiling** — *where is time going?* Run `gbfs_lazy` or `astar_eager` (`BUILD_EXECUTABLES=ON`) under `valgrind --tool=callgrind` and inspect with `kcachegrind` or `callgrind_annotate`.

A methodology layer for trustworthy, comparable wall-time numbers lives in [`profiling/measure.sh`](../profiling/measure.sh), [`profiling/compare.sh`](../profiling/compare.sh), and [`profiling/baselines/`](../profiling/baselines/) — see *Reproducible measurement & comparison* below.

## Wall-time: projection_generator

Reads instance lists from [`profiling/projection_generator.json`](../profiling/projection_generator.json) (47 instances across 12 PDDL domains, all resolved under [`data/`](../data/)).

List all registered benchmarks:

```bash
./build/profiling/projection_generator --benchmark_list_tests=true
```

Run a filtered subset and capture results as JSON:

```bash
./build/profiling/projection_generator \
    --benchmark_filter="BM_ProjectionGenerator/(labyrinth/p01|genome-edit-distance/d-1-2)$" \
    --benchmark_out=results.json \
    --benchmark_out_format=json
```

Useful flags:

- `--benchmark_min_time=1x` — single iteration, for smoke-testing the binary.
- `--benchmark_repetitions=N --benchmark_report_aggregates_only=true` — statistical run; reports mean / median / stddev across N repeats.

If the binary prints `***WARNING*** CPU scaling is enabled` on startup, treat the numbers as soft (~5–15% noise on a typical desktop). Pin the governor and bind to an isolated core (TODO below) for trustworthy comparisons.

## Function profiling: callgrind on a planner

`gbfs_lazy` and `astar_eager` are the standard entry points. The `-N 1` flag expands at most one node, which is enough to capture the hot path of successor generation while keeping the trace bounded.

Whole-program profile:

```bash
valgrind \
    --tool=callgrind \
    --collect-jumps=yes \
    --dump-instr=yes \
    ./build/exe/gbfs_lazy \
        -D data/rovers-large-simple/domain.pddl \
        -P data/rovers-large-simple/p-r1-w1000-o1-1-g2.pddl \
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
        -D data/rovers-large-simple/domain.pddl \
        -P data/rovers-large-simple/p-r1-w1000-o1-1-g2.pddl \
        -O plan.txt \
        -N 1
```

`--toggle-collect` takes a mangled C++ symbol. To find one, grep the binary's symbol table for the function name — Itanium ABI mangling preserves the identifier as a substring:

```bash
nm build/exe/gbfs_lazy | grep set_next_assignment_sets
```

## Reproducible measurement & comparison

The methodology layer turns raw wall-time numbers into evidence. Three artifacts:

| Artifact | Role |
|---|---|
| `sudo pyperf system tune` | System prep, once per measurement session. Sets governor=performance, disables turbo + ASLR, suppresses perf-counter noise. Reverse with `sudo pyperf system reset`. Not wrapped — knowing what state your system is in is the user's job. |
| [`profiling/measure.sh`](../profiling/measure.sh) | Per-run discipline wrapper. Pins to a CPU core via `taskset`, sets `MALLOC_ARENA_MAX=1` and `LANG=C` (matching `experiments/{gbfs_lazy,astar_eager}/*.sh`). If it didn't go through this, it didn't measure. |
| [`profiling/baselines/`](../profiling/baselines/) | Committed Google Benchmark JSON per reference machine, machine-tagged. See its [README](../profiling/baselines/README.md) for capture and refresh procedure. |
| [`profiling/compare.sh`](../profiling/compare.sh) | Wrapper around Google Benchmark's `tools/compare.py`. Takes a baseline JSON + a new JSON, prints per-instance deltas with statistical context. |

Typical session:

```bash
sudo pyperf system tune

./profiling/measure.sh ./build/profiling/projection_generator \
    --benchmark_filter="BM_ProjectionGenerator/.*" \
    --benchmark_repetitions=5 \
    --benchmark_report_aggregates_only=true \
    --benchmark_out=/tmp/new.json \
    --benchmark_out_format=json

./profiling/compare.sh profiling/baselines/<machine-tag>.json /tmp/new.json

sudo pyperf system reset
```

Treat per-instance deltas inside ±2σ of the baseline stddev as noise; investigate beyond that. If `compare.py` reports a regression you weren't expecting, the methodology has done its job — read the diff before changing the baseline.
