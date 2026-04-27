#!/usr/bin/env python3
import argparse
import datetime as dt
import json
import os
import pathlib
import platform
import re
import subprocess
import sys

from schema import validate_suite


RESULT_RE = re.compile(
    r"^\s*\d+/\d+\s+Test\s+#\d+:\s+(?P<name>.*?)\s+\.{2,}\s*"
    r"(?P<status>Passed|\*\*\*Timeout|\*\*\*Failed|Failed)\s+(?P<duration>[0-9.]+)\s+sec\b"
)


def run_text(command: list[str], cwd: pathlib.Path | None = None):
    result = subprocess.run(command, cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    if result.returncode != 0:
        return None
    return result.stdout.strip()


def find_cmake_cache(path: pathlib.Path):
    for candidate in [path, *path.parents]:
        cache = candidate / "CMakeCache.txt"
        if cache.exists():
            return cache
    return None


def read_cmake_cache(cache_path: pathlib.Path | None):
    if cache_path is None:
        return {}

    result = {}
    wanted = {
        "CMAKE_BUILD_TYPE",
        "CMAKE_CXX_COMPILER",
        "CMAKE_CXX_COMPILER_ID",
        "CMAKE_CXX_COMPILER_VERSION",
        "CMAKE_CXX_FLAGS",
    }

    for line in cache_path.read_text(errors="replace").splitlines():
        if line.startswith("//") or line.startswith("#") or "=" not in line:
            continue
        key_with_type, value = line.split("=", 1)
        key = key_with_type.split(":", 1)[0]
        if key in wanted:
            result[key] = value

    result["CMakeCache"] = str(cache_path)
    return result


def read_cmake_compiler_metadata(cache_path: pathlib.Path | None):
    if cache_path is None:
        return {}

    cmake_files = cache_path.parent / "CMakeFiles"
    compiler_files = list(cmake_files.glob("*/CMakeCXXCompiler.cmake"))
    if not compiler_files:
        return {}

    result = {}
    wanted = {
        "CMAKE_CXX_COMPILER_ID",
        "CMAKE_CXX_COMPILER_VERSION",
    }
    pattern = re.compile(r'set\((?P<key>[A-Za-z0-9_]+)\s+"(?P<value>.*)"\)')

    for line in compiler_files[0].read_text(errors="replace").splitlines():
        match = pattern.match(line)
        if match and match.group("key") in wanted:
            result[match.group("key")] = match.group("value")

    return result


def collect_metadata(args, executable: pathlib.Path, test_dir: pathlib.Path, output_dir: pathlib.Path, ctest_command: list[str]):
    cache_path = find_cmake_cache(test_dir)
    return {
        "git": {
            "commit": run_text(["git", "rev-parse", "HEAD"]),
            "branch": run_text(["git", "rev-parse", "--abbrev-ref", "HEAD"]),
            "dirty": bool(run_text(["git", "status", "--porcelain"])),
        },
        "build": {
            "executable": str(executable),
            "test_dir": str(test_dir),
            **read_cmake_cache(cache_path),
            **read_cmake_compiler_metadata(cache_path),
        },
        "host": {
            "hostname": platform.node(),
            "platform": platform.platform(),
            "cpu_count": os.cpu_count(),
        },
        "runner": {
            "command": sys.argv,
            "ctest_command": ctest_command,
            "suite_json": str(args.suite_json),
            "output_dir": str(output_dir),
            "parallel": args.parallel,
            "benchmark_min_time": args.benchmark_min_time,
            "benchmark_repetitions": args.benchmark_repetitions,
            "benchmark_report_aggregates_only": args.benchmark_report_aggregates_only,
        },
    }


def parse_results(log: str):
    results = {}

    for line in log.splitlines():
        match = RESULT_RE.match(line)
        if not match:
            continue

        name = match.group("name").strip()
        status = match.group("status")
        duration = float(match.group("duration"))

        if status == "Passed":
            parsed_status = "passed"
        elif status == "***Timeout":
            parsed_status = "timed_out"
        else:
            parsed_status = "failed"

        results[name] = {
            "status": parsed_status,
            "ctest_duration_seconds": duration,
        }

    return results


def load_suite(suite_json: pathlib.Path):
    suite = json.loads(suite_json.read_text())
    validate_suite(suite)
    return suite


def load_cases(suite):
    cases = []

    for domain_name, domain_config in suite["domains"].items():
        for task_name, task_file in domain_config["tasks"].items():
            cases.append(
                {
                    "run_name": f"{domain_name}/{task_name}",
                    "domain": domain_name,
                    "task": task_name,
                    "domain_file": domain_config["domain_file"],
                    "problem_file": task_file,
                }
            )

    return cases


def group_cases(cases):
    groups = {"passed": [], "timed_out": [], "failed": [], "not_run": []}
    for case in cases:
        groups[case["status"]].append(case)
    return groups


def run_benchmarks(
    benchmark_executable: pathlib.Path,
    output_dir: pathlib.Path,
    passed_cases,
    min_time: str,
    repetitions: int | None,
    report_aggregates_only: bool,
):
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
            if repetitions is not None:
                command.append(f"--benchmark_repetitions={repetitions}")
            if report_aggregates_only:
                command.append("--benchmark_report_aggregates_only=true")

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
            case["benchmark_status"] = "passed" if result.returncode == 0 else "failed"
            case["benchmark_result_file"] = str(result_file)
            case["benchmark_exit_code"] = result.returncode

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
        help="Profiling suite JSON with domains, domain files, and task files.",
    )
    parser.add_argument("--benchmark-min-time", default="0.1s", help="Google Benchmark --benchmark_min_time value.")
    parser.add_argument("--benchmark-repetitions", type=int, help="Google Benchmark --benchmark_repetitions value.")
    parser.add_argument(
        "--benchmark-report-aggregates-only",
        action="store_true",
        help="Forward --benchmark_report_aggregates_only=true to Google Benchmark.",
    )
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

    metadata = collect_metadata(args, executable, test_dir, output_dir, command)

    result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    log_file.write_text(result.stdout)

    ctest_results = parse_results(result.stdout)

    suite = load_suite(args.suite_json)
    cases = load_cases(suite)

    for case in cases:
        ctest_result = ctest_results.get(case["run_name"])
        if ctest_result is None:
            case["status"] = "not_run"
            case["benchmark_status"] = "skipped"
            case["benchmark_skip_reason"] = "ctest_not_run"
        else:
            case.update(ctest_result)
            if case["status"] == "timed_out":
                case["benchmark_status"] = "skipped"
                case["benchmark_skip_reason"] = "ctest_timed_out"
            elif case["status"] == "failed":
                case["benchmark_status"] = "skipped"
                case["benchmark_skip_reason"] = "ctest_failed"

    groups = group_cases(cases)
    benchmark_results = run_benchmarks(
        executable,
        output_dir,
        groups["passed"],
        args.benchmark_min_time,
        args.benchmark_repetitions,
        args.benchmark_report_aggregates_only,
    )

    summary = {
        "generated": dt.datetime.now().astimezone().isoformat(timespec="seconds"),
        "metadata": metadata,
        "attributes": suite.get("attributes", {}),
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
