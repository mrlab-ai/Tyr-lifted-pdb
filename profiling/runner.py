#!/usr/bin/env python3
import argparse
import datetime as dt
import json
import pathlib
import re
import subprocess
import sys


RESULT_RE = re.compile(
    r"^\s*\d+/\d+\s+Test\s+#\d+:\s+(?P<name>.*?)\s+\.{2,}\s*(?P<status>Passed|\*\*\*Timeout|\*\*\*Failed|Failed)\b"
)


def parse_results(log: str):
    statuses = {}

    for line in log.splitlines():
        match = RESULT_RE.match(line)
        if not match:
            continue

        name = match.group("name").strip()
        status = match.group("status")

        if status == "Passed":
            statuses[name] = "passed"
        elif status == "***Timeout":
            statuses[name] = "timed_out"
        else:
            statuses[name] = "failed"

    return statuses


def load_cases(suite_json: pathlib.Path):
    suite = json.loads(suite_json.read_text())
    cases = []

    for run_name, config in suite["cases"].items():
        cases.append(
            {
                "run_name": run_name,
                "domain_file": config["domain_file"],
                "problem_file": config["problem_file"],
            }
        )

    return cases


def group_cases(cases):
    groups = {"passed": [], "timed_out": [], "failed": [], "not_run": []}
    for case in cases:
        groups[case["status"]].append(case)
    return groups


def run_benchmarks(benchmark_executable: pathlib.Path, output_dir: pathlib.Path, passed_cases, min_time: str):
    results = []
    benchmark_output_dir = output_dir / "benchmark-results"
    benchmark_log_file = output_dir / "benchmark.log"

    with benchmark_log_file.open("w") as benchmark_log:
        for case in passed_cases:
            run_name = case["run_name"]
            result_file = benchmark_output_dir / f"{run_name}.json"
            result_file.parent.mkdir(parents=True, exist_ok=True)

            command = [
                str(benchmark_executable),
                f"--benchmark_filter=^{run_name}/.*",
                f"--benchmark_min_time={min_time}",
                "--benchmark_format=json",
                f"--benchmark_out={result_file}",
                "--benchmark_out_format=json",
            ]

            result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            benchmark_log.write(result.stdout)
            if result.stdout and not result.stdout.endswith("\n"):
                benchmark_log.write("\n")

            results.append(
                {
                    "run_name": run_name,
                    "command": command,
                    "exit_code": result.returncode,
                    "result_file": str(result_file),
                }
            )

    return results


def main():
    parser = argparse.ArgumentParser(
        description="Run CTest profiling, summarize outcomes, and benchmark the runs that passed the CTest screen."
    )
    parser.add_argument("--executable", type=pathlib.Path, required=True, help="Profiling benchmark executable to run.")
    parser.add_argument(
        "--output-dir",
        required=True,
        help="Directory for ctest.log and ctest-summary.json.",
    )
    parser.add_argument("-j", "--parallel", default="24", help="CTest parallelism.")
    parser.add_argument(
        "--suite-json",
        type=pathlib.Path,
        required=True,
        help="Profiling suite JSON with run names, domain files, and problem files.",
    )
    parser.add_argument("--benchmark-min-time", default="0.1s", help="Google Benchmark --benchmark_min_time value.")
    parser.add_argument(
        "ctest_args",
        nargs=argparse.REMAINDER,
        help="Extra arguments passed to ctest after '--'.",
    )
    args = parser.parse_args()

    extra_args = args.ctest_args
    if extra_args and extra_args[0] == "--":
        extra_args = extra_args[1:]

    output_dir = pathlib.Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    executable = args.executable
    test_dir = executable.parent
    log_file = output_dir / "ctest.log"
    summary_file = output_dir / "ctest-summary.json"

    command = [
        "ctest",
        "--test-dir",
        str(test_dir),
        "--output-on-failure",
        f"-j{args.parallel}",
        *extra_args,
    ]

    result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    log_file.write_text(result.stdout)

    statuses = parse_results(result.stdout)

    cases = load_cases(args.suite_json)

    for case in cases:
        case["status"] = statuses.get(case["run_name"], "not_run")

    groups = group_cases(cases)
    benchmark_results = run_benchmarks(executable, output_dir, groups["passed"], args.benchmark_min_time)

    summary = {
        "generated": dt.datetime.now().astimezone().isoformat(timespec="seconds"),
        "command": command,
        "exit_code": result.returncode,
        "cases": cases,
        "passed": groups["passed"],
        "timed_out": groups["timed_out"],
        "failed": groups["failed"],
        "not_run": groups["not_run"],
        "benchmark_results": benchmark_results,
        "counts": {
            "passed": len(groups["passed"]),
            "timed_out": len(groups["timed_out"]),
            "failed": len(groups["failed"]),
            "not_run": len(groups["not_run"]),
            "total": len(cases),
        },
    }

    rendered_summary = json.dumps(summary, indent=2)
    summary_file.write_text(rendered_summary + "\n")
    print(rendered_summary)

    has_benchmark_failures = any(benchmark_result["exit_code"] != 0 for benchmark_result in benchmark_results)
    if groups["failed"] or groups["not_run"] or has_benchmark_failures:
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
