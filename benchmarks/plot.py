#!/usr/bin/env python3
"""
Plot Meta regex benchmark CSVs.

Expected CSV columns from the revised tester:

    known_pairs: suite,case_name,extract,matcher,total_cases,run_cases,time
    fuzzy:       suite,max_input_len,extract,matcher,total_cases,run_cases,time
    files:       suite,file_name,extract,matcher,total_cases,run_cases,time

LIBC is emitted as a normal matcher row, so all plots include libc timings.

Usage:
    python benchmarks/plot_benchmarks.py benchmarks/
    python benchmarks/plot_benchmarks.py benchmarks/known_pairs-123.csv benchmarks/fuzzy-123.csv
    python benchmarks/plot_benchmarks.py benchmarks/ --out-dir benchmarks/plots
"""

from __future__ import annotations

import argparse
import csv
import sys
from collections import defaultdict
from pathlib import Path
from typing import Iterable

import matplotlib.pyplot as plt

REQUIRED_COLUMNS = {"suite", "extract", "matcher", "total_cases", "run_cases", "time"}
CASE_COLUMNS = ("case_name", "max_input_len", "file_name")
MATCHER_ORDER = ["LIBC", "BTNFA", "TNFA", "TDFA", "LAZY_DFA", "STATIC_DFA"]


def parse_bool(value: str) -> bool:
    return str(value).strip().lower() in {"1", "true", "yes", "y", "extract"}


def parse_int(value: str, default: int = 0) -> int:
    try:
        return int(str(value).strip())
    except Exception:
        return default


def parse_float(value: str, default: float = 0.0) -> float:
    try:
        return float(str(value).strip())
    except Exception:
        return default


def matcher_sort_key(name: str) -> tuple[int, str]:
    try:
        return (MATCHER_ORDER.index(name), name)
    except ValueError:
        return (len(MATCHER_ORDER), name)


def discover_csvs(paths: Iterable[Path]) -> list[Path]:
    result: list[Path] = []
    for path in paths:
        if path.is_dir():
            result.extend(sorted(path.glob("*.csv")))
        elif path.is_file():
            result.append(path)
        else:
            print(f"warning: skipping missing path: {path}", file=sys.stderr)
    return result


def read_rows(path: Path) -> tuple[list[dict[str, str]], str]:
    with path.open("r", newline="") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames is None:
            raise ValueError(f"{path}: missing CSV header")
        columns = set(reader.fieldnames)
        missing = REQUIRED_COLUMNS - columns
        if missing:
            raise ValueError(f"{path}: missing required columns: {sorted(missing)}")
        case_col = next((c for c in CASE_COLUMNS if c in columns), None)
        if case_col is None:
            raise ValueError(f"{path}: cannot identify case column")
        return list(reader), case_col


def save_fig(out_path: Path) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    plt.tight_layout()
    plt.savefig(out_path, dpi=160)
    plt.close()
    print(out_path)


def safe_name(text: str) -> str:
    return "".join(ch if ch.isalnum() or ch in "._-" else "_" for ch in text).strip("_") or "plot"


def rows_by_extract(rows: list[dict[str, str]]) -> dict[bool, list[dict[str, str]]]:
    grouped: dict[bool, list[dict[str, str]]] = {False: [], True: []}
    for row in rows:
        grouped[parse_bool(row["extract"])].append(row)
    return grouped


def plot_known_pairs(path: Path, rows: list[dict[str, str]], case_col: str, out_dir: Path) -> None:
    for extract, subrows in rows_by_extract(rows).items():
        if not subrows:
            continue
        grouped: dict[str, dict[str, dict[str, str]]] = defaultdict(dict)
        for row in subrows:
            grouped[row[case_col]][row["matcher"]] = row
        cases = sorted(grouped.keys())
        matchers = sorted({row["matcher"] for row in subrows}, key=matcher_sort_key)
        x = list(range(len(cases)))
        width = 0.8 / max(1, len(matchers))
        plt.figure(figsize=(max(10, len(cases) * 0.42), 6))
        for mi, matcher in enumerate(matchers):
            values = [parse_float(grouped[case].get(matcher, {}).get("time", "0")) for case in cases]
            offsets = [v + (mi - (len(matchers) - 1) / 2) * width for v in x]
            plt.bar(offsets, values, width=width, label=matcher)
        suffix = "extract" if extract else "no_extract"
        plt.title(f"Known pairs runtime ({suffix})")
        plt.xlabel("test suite")
        plt.ylabel("seconds")
        plt.xticks(x, cases, rotation=45, ha="right")
        plt.legend()
        plt.grid(axis="y", alpha=0.3)
        save_fig(out_dir / f"{path.stem}-known-pairs-{suffix}.png")


def plot_fuzzy(path: Path, rows: list[dict[str, str]], case_col: str, out_dir: Path) -> None:
    for extract, subrows in rows_by_extract(rows).items():
        if not subrows:
            continue
        by_matcher: dict[str, list[dict[str, str]]] = defaultdict(list)
        for row in subrows:
            by_matcher[row["matcher"]].append(row)
        plt.figure(figsize=(10, 6))
        for matcher in sorted(by_matcher, key=matcher_sort_key):
            points = sorted(
                ((parse_int(row[case_col]), parse_float(row["time"])) for row in by_matcher[matcher]),
                key=lambda x: x[0],
            )
            if points:
                xs, ys = zip(*points)
                plt.plot(xs, ys, marker="o", label=matcher)
        suffix = "extract" if extract else "no_extract"
        plt.title(f"Fuzzy runtime ({suffix})")
        plt.xlabel("max input length")
        plt.ylabel("seconds")
        plt.xscale("log", base=2)
        plt.yscale("log")
        plt.legend()
        plt.grid(True, which="both", alpha=0.3)
        save_fig(out_dir / f"{path.stem}-fuzzy-runtime-{suffix}.png")

        plt.figure(figsize=(10, 6))
        for matcher in sorted(by_matcher, key=matcher_sort_key):
            points = []
            for row in by_matcher[matcher]:
                run_cases = max(1, parse_int(row["run_cases"]))
                points.append((parse_int(row[case_col]), parse_float(row["time"]) / run_cases))
            points.sort(key=lambda x: x[0])
            if points:
                xs, ys = zip(*points)
                plt.plot(xs, ys, marker="o", label=matcher)
        plt.title(f"Fuzzy time per executed case ({suffix})")
        plt.xlabel("max input length")
        plt.ylabel("seconds / executed case")
        plt.xscale("log", base=2)
        plt.yscale("log")
        plt.legend()
        plt.grid(True, which="both", alpha=0.3)
        save_fig(out_dir / f"{path.stem}-fuzzy-per-case-{suffix}.png")


def plot_files(path: Path, rows: list[dict[str, str]], case_col: str, out_dir: Path) -> None:
    for extract, subrows in rows_by_extract(rows).items():
        if not subrows:
            continue
        grouped: dict[str, dict[str, dict[str, str]]] = defaultdict(dict)
        for row in subrows:
            grouped[row[case_col]][row["matcher"]] = row
        files = sorted(grouped.keys())
        matchers = sorted({row["matcher"] for row in subrows}, key=matcher_sort_key)
        x = list(range(len(files)))
        width = 0.8 / max(1, len(matchers))
        plt.figure(figsize=(max(10, len(files) * 0.5), 6))
        for mi, matcher in enumerate(matchers):
            values = [parse_float(grouped[file].get(matcher, {}).get("time", "0")) for file in files]
            offsets = [v + (mi - (len(matchers) - 1) / 2) * width for v in x]
            plt.bar(offsets, values, width=width, label=matcher)
        suffix = "extract" if extract else "no_extract"
        plt.title(f"File-input runtime ({suffix})")
        plt.xlabel("input file")
        plt.ylabel("seconds")
        plt.xticks(x, files, rotation=45, ha="right")
        plt.legend()
        plt.grid(axis="y", alpha=0.3)
        save_fig(out_dir / f"{path.stem}-files-runtime-{suffix}.png")

        plt.figure(figsize=(max(10, len(files) * 0.5), 6))
        for mi, matcher in enumerate(matchers):
            values = []
            for file in files:
                row = grouped[file].get(matcher)
                if row is None:
                    values.append(0.0)
                else:
                    values.append(parse_float(row["time"]) / max(1, parse_int(row["run_cases"])))
            offsets = [v + (mi - (len(matchers) - 1) / 2) * width for v in x]
            plt.bar(offsets, values, width=width, label=matcher)
        plt.title(f"File-input time per executed regex ({suffix})")
        plt.xlabel("input file")
        plt.ylabel("seconds / executed regex")
        plt.xticks(x, files, rotation=45, ha="right")
        plt.legend()
        plt.grid(axis="y", alpha=0.3)
        save_fig(out_dir / f"{path.stem}-files-per-regex-{suffix}.png")


def plot_csv(path: Path, out_dir: Path) -> None:
    rows, case_col = read_rows(path)
    if not rows:
        print(f"warning: no rows in {path}", file=sys.stderr)
        return
    suite = rows[0].get("suite", "")
    stem = path.stem.lower()
    if suite == "known_pairs" or "known" in stem:
        plot_known_pairs(path, rows, case_col, out_dir)
    elif suite == "fuzzy" or "fuzzy" in stem:
        plot_fuzzy(path, rows, case_col, out_dir)
    elif suite == "file_fuzzy" or "file" in stem:
        plot_files(path, rows, case_col, out_dir)
    else:
        raise ValueError(f"{path}: unknown suite {suite!r}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Plot Meta regex benchmark CSVs")
    parser.add_argument("paths", nargs="+", type=Path, help="CSV files or directories containing CSVs")
    parser.add_argument("--out-dir", type=Path, default=None, help="directory for PNG output")
    args = parser.parse_args()

    csvs = discover_csvs(args.paths)
    if not csvs:
        print("no CSV files found", file=sys.stderr)
        return 1
    for path in csvs:
        out_dir = args.out_dir if args.out_dir is not None else path.parent
        try:
            plot_csv(path, out_dir)
        except Exception as exc:
            print(f"error: {exc}", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
