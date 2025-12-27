# run_recordsize_experiment.py
#
# Orchestrates Brendan Gregg-style "record size" experiment:
#   - same total bytes read
#   - progressively smaller record sizes
#   - measure syscall + perf stats

import subprocess
import time
import csv
from pathlib import Path

FS_BENCH = "./fs_bench"
DATA_FILE = "data.bin"

# Total bytes per run (same workload).
# 8 * 1024**3 = 8 GiB; adjust if you like.
TOTAL_BYTES = 8 * 1024**3

# Record sizes: from large to small
RECORD_SIZES = [
    1024 * 1024,   # 1 MiB
    256 * 1024,    # 256 KiB
    64 * 1024,     # 64 KiB
    16 * 1024,     # 16 KiB
    4 * 1024,      # 4 KiB
]

MODE = "rand"   # or "seq"
SEED = 12345

PERF_EVENTS = [
    "cycles",
    "instructions",
    "cache-misses",
    "major-faults",
    "minor-faults",
    "cs",  # context-switches
]

OUTPUT_CSV = "results.csv"


def run_simple(cmd):
    t0 = time.time()
    proc = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    t1 = time.time()
    return proc, t1 - t0


def parse_strace_c_output(stderr_text):
    total_time = 0.0
    total_calls = 0
    lines = stderr_text.splitlines()
    started = False
    for line in lines:
        line = line.strip()
        if not line:
            continue
        if line.startswith("% time"):
            started = True
            continue
        if not started:
            continue
        if line.startswith("------") or line.startswith("syscall"):
            continue
        parts = line.split()
        if len(parts) < 5:
            continue
        try:
            seconds = float(parts[1])
            calls = int(parts[3])
        except ValueError:
            continue
        total_time += seconds
        total_calls += calls
    return total_calls, total_time


def parse_perf_stat_output(stderr_text):
    metrics = {}
    for line in stderr_text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(",")
        if len(parts) < 3:
            continue
        value_str, unit, event = parts[0].strip(), parts[1].strip(), parts[2].strip()
        if "<not" in value_str:
            continue
        try:
            if value_str.endswith("K"):
                val = float(value_str[:-1]) * 1e3
            elif value_str.endswith("M"):
                val = float(value_str[:-1]) * 1e6
            elif value_str.endswith("G"):
                val = float(value_str[:-1]) * 1e9
            else:
                val = float(value_str)
        except ValueError:
            continue
        metrics[event] = val
    return metrics


def main():
    if not Path(FS_BENCH).is_file():
        raise SystemExit("fs_bench not found. Build fs_bench.c first.")

    if not Path(DATA_FILE).is_file():
        raise SystemExit(
            f"{DATA_FILE} not found. Create it, e.g.: fallocate -l 16G {DATA_FILE}"
        )

    with open(OUTPUT_CSV, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "record_size",
            "total_bytes",
            "mode",
            "wall_time_sec",
            "strace_syscalls",
            "strace_syscall_time_sec",
            "perf_cycles",
            "perf_instructions",
            "perf_cache_misses",
            "perf_major_faults",
            "perf_minor_faults",
            "perf_context_switches",
        ])

        for record_size in RECORD_SIZES:
            print(f"Running record_size={record_size} bytes")

            base_cmd = [
                FS_BENCH,
                "--file", DATA_FILE,
                "--mode", MODE,
                "--record-size", str(record_size),
                "--total-bytes", str(TOTAL_BYTES),
                "--seed", str(SEED),
            ]

            # 1) Plain run for wall time
            proc, wall_time = run_simple(base_cmd)
            if proc.returncode != 0:
                print("  fs_bench failed:", proc.stderr.strip())
                continue

            # 2) strace -c
            s_proc = subprocess.run(
                ["strace", "-c"] + base_cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            strace_calls, strace_time = parse_strace_c_output(s_proc.stderr)

            # 3) perf stat
            p_proc = subprocess.run(
                ["perf", "stat", "-x", ",", "-e", ",".join(PERF_EVENTS), "--"] + base_cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            perf_metrics = parse_perf_stat_output(p_proc.stderr)

            cycles = perf_metrics.get("cycles", 0.0)
            instr = perf_metrics.get("instructions", 0.0)
            cache_misses = perf_metrics.get("cache-misses", 0.0)
            majflt = perf_metrics.get("major-faults", 0.0)
            minflt = perf_metrics.get("minor-faults", 0.0)
            cs = perf_metrics.get("cs", 0.0)

            writer.writerow([
                record_size,
                TOTAL_BYTES,
                MODE,
                f"{wall_time:.6f}",
                strace_calls,
                f"{strace_time:.6f}",
                int(cycles),
                int(instr),
                int(cache_misses),
                int(majflt),
                int(minflt),
                int(cs),
            ])
            f.flush()


if __name__ == "__main__":
    main()
