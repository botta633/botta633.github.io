#!/usr/bin/env python3
"""
plot_fs_bench.py

Usage:
    python plot_fs_bench.py results.csv
"""

import csv
import sys
import math
import matplotlib.pyplot as plt


def read_results(path):
    record_size = []
    wall_time = []
    syscall_time = []
    syscalls = []

    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            record_size.append(int(row["record_size"]))
            wall_time.append(float(row["wall_time_sec"]))
            syscall_time.append(float(row["strace_syscall_time_sec"]))
            syscalls.append(int(row["strace_syscalls"]))

    return record_size, wall_time, syscall_time, syscalls


def main():
    if len(sys.argv) != 2:
        print("Usage: python plot_fs_bench.py results.csv")
        sys.exit(1)

    csv_path = sys.argv[1]
    record_size, wall_time, syscall_time, syscalls = read_results(csv_path)

    # Sort by record_size so lines are nice and monotonic
    zipped = sorted(
        zip(record_size, wall_time, syscall_time, syscalls),
        key=lambda x: x[0]
    )
    record_size, wall_time, syscall_time, syscalls = map(list, zip(*zipped))

    # Convert record size to KiB for nicer labels
    record_size_kib = [rs / 1024 for rs in record_size]

    # ---------- Figure 1: wall time + syscall time ----------
    plt.figure(figsize=(8, 5))
    plt.title("Wall time vs syscall time vs record size")
    plt.xlabel("Record size (KiB, log scale)")
    plt.xscale("log", base=2)

    plt.plot(record_size_kib, wall_time, marker="o", label="Wall time (s)")
    plt.plot(record_size_kib, syscall_time, marker="s", label="strace syscall time (s)")

    plt.grid(True, which="both", linestyle="--", alpha=0.4)
    plt.legend()
    plt.tight_layout()
    plt.savefig("fs_bench_time_vs_record_size.png", dpi=200)

    # ---------- Figure 2: syscall count ----------
    plt.figure(figsize=(8, 5))
    plt.title("Syscall count vs record size")
    plt.xlabel("Record size (KiB, log scale)")
    plt.ylabel("strace_syscalls")
    plt.xscale("log", base=2)

    plt.plot(record_size_kib, syscalls, marker="o")
    plt.grid(True, which="both", linestyle="--", alpha=0.4)
    plt.tight_layout()
    plt.savefig("fs_bench_syscalls_vs_record_size.png", dpi=200)

    print("Saved:")
    print("  fs_bench_time_vs_record_size.png")
    print("  fs_bench_syscalls_vs_record_size.png")


if __name__ == "__main__":
    main()