# Profiling

Profiling code lives next to a small JSON suite file. The JSON file selects the
benchmark instances, while CMake reads that JSON and creates one CTest test per
profiling task. This keeps each task in its own process with its own timeout.

## Data Layout

Keep benchmark instances in the same domain directory:

```text
data/<category>/<domain>/
  domain.pddl
  profiling-1.pddl
  profiling-2.pddl
```

## Suite JSON

Example:

```json
{
  "cases": {
    "blocksworld-large-simple/profiling-1": {
      "domain_file": "classical/blocksworld-large-simple/domain.pddl",
      "problem_file": "classical/blocksworld-large-simple/profiling-1.pddl"
    }
  }
}
```

Paths are relative to `data/`.

## Build

Configure with profiling enabled:

```bash
cmake -S . -B build -DBUILD_PROFILING=ON
cmake --build build --target successor_generator -j24
```

## Run

Use the runner as the main entry point for a profiling experiment:

```bash
profiling/runner.py \
  --executable build/profiling/planning/lifted_task/successor_generator \
  --output-dir profiling-results/planning/lifted_task/successor_generator \
  --suite-json profiling/planning/lifted_task/successor_generator.json \
  --benchmark-min-time 0.1s
```

This writes:

```text
profiling-results/planning/lifted_task/successor_generator/ctest.log
profiling-results/planning/lifted_task/successor_generator/ctest-summary.json
profiling-results/planning/lifted_task/successor_generator/benchmark.log
profiling-results/planning/lifted_task/successor_generator/benchmark-results/<run_name>.json
```

The runner first uses CTest as a timeout screen. It then runs Google Benchmark
only for the cases that passed the CTest screen, so timed-out runs do not produce
partial benchmark JSON files.

CTest `TIMEOUT` is a hard wall-clock limit for each test process.
`--benchmark-min-time` is forwarded to Google Benchmark and controls sampling
time after a benchmark iteration returns.

## Debugging

Use CTest or Google Benchmark directly when debugging registration or a single
case:

```bash
ctest --test-dir build/profiling/planning/lifted_task -N
build/profiling/planning/lifted_task/successor_generator --benchmark_list_tests
```
