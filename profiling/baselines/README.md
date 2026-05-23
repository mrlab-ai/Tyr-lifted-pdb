# Baselines

Recorded reference timings for `projection_generator`, used as the "before" side of perf comparisons via [`profiling/compare.sh`](../compare.sh).

## Layout

- `<machine-tag>.json` — Google Benchmark JSON output, captured through [`profiling/measure.sh`](../measure.sh) after `sudo pyperf system tune`, on the named reference machine.

The machine tag is the honesty: a baseline only means something against a declared CPU. Pick a short, stable label per reference host (`liu-workstation`, `lab-rack-3`, etc.) and capture under it.

## Capturing a baseline

```bash
# Once per measurement session — reverse with `sudo pyperf system reset`.
sudo pyperf system tune

# Pinned to core 2 (override via MEASURE_CORE), 5 repeats, aggregates only.
./profiling/measure.sh ./build/profiling/projection_generator \
    --benchmark_filter="BM_ProjectionGenerator/.*" \
    --benchmark_repetitions=5 \
    --benchmark_report_aggregates_only=true \
    --benchmark_out=profiling/baselines/<machine-tag>.json \
    --benchmark_out_format=json
```

Trim the filter to a subset when capturing a baseline for a focused experiment; commit the result under a descriptive name (e.g. `<machine-tag>.<focus>.json`).

## Refresh policy

Baselines are static, not auto-regenerated. Refresh on:

1. **New instances added** to `projection_generator.json`.
2. **Reference machine swap** — commit under a new tag rather than overwriting.
3. **Explicit acceptance** of a perf change as the new normal (intentional optimization or knowingly accepted regression), called out in the commit message.

Never refresh silently. A baseline that drifts with every run is no baseline.

## Comparing a new run

```bash
./profiling/measure.sh ./build/profiling/projection_generator \
    --benchmark_filter="BM_ProjectionGenerator/.*" \
    --benchmark_repetitions=5 \
    --benchmark_report_aggregates_only=true \
    --benchmark_out=/tmp/new.json \
    --benchmark_out_format=json

./profiling/compare.sh profiling/baselines/<machine-tag>.json /tmp/new.json
```

Output lists per-instance deltas with statistical context. Treat changes inside ±2σ of baseline stddev as noise; investigate beyond that.
