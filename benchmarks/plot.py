#!/usr/bin/env python3
"""
Plot Meta regex benchmark CSVs.

Expected CSV columns from the reorganized tester:

    suite,case_name/max_input_len/file_name,extract,matcher,total_cases,
    run_cases,skip_cases,libc_time,matcher_time,mismatches

Usage examples:

    python benchmarks/plot.py benchmarks/known_pairs-123.csv
    python benchmarks/plot.py benchmarks/fuzzy-123.csv benchmarks/files-123.csv
    python benchmarks/plot.py benchmarks/
    python benchmarks/plot.py benchmarks/ --out-dir benchmarks/plots

Outputs PNG files, one or more per CSV.
"""

from __future__ import annotations

import argparse
import csv
import math
import sys
from collections import defaultdict
from pathlib import Path
from typing import Iterable

import matplotlib.pyplot as plt

REQUIRED_COLUMNS = {
    "suite",
    "extract",
    "matcher",
    "total_cases",
    "run_cases",
    "skip_cases",
    "libc_time",
    "matcher_time",
    "mismatches",
}

CASE_COLUMNS = ("case_name", "max_input_len", "file_name")

MATCHER_ORDER = ["BTNFA", "TNFA", "TDFA", "LAZY_DFA", "STATIC_DFA"]


def parse_bool(value: str) -> bool:
    value = str(value).strip().lower()
    return value in {"1", "true", "yes", "y", "extract"}


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
            # Backward-compatible fallback: the older code used a generic second
            # column name in a comment. Use the second column if it exists.
            if len(reader.fieldnames) >= 2:
                case_col = reader.fieldnames[1]
            else:
                raise ValueError(f"{path}: cannot identify case column")

        rows = list(reader)
    return rows, case_col


def safe_name(text: str) -> str:
    out = []
    for ch in text:
        if ch.isalnum() or ch in "._-":
            out.append(ch)
        else:
            out.append("_")
    name = "".join(out).strip("_")
    return name or "plot"


def save_fig(out_path: Path) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    plt.tight_layout()
    plt.savefig(out_path, dpi=160)
    plt.close()
    print(out_path)


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
            values = []
            for case in cases:
                row = grouped[case].get(matcher)
                if row is None or parse_int(row["run_cases"]) == 0:
                    values.append(0.0)
                else:
                    values.append(parse_float(row["matcher_time"]))
            offsets = [v + (mi - (len(matchers) - 1) / 2) * width for v in x]
            plt.bar(offsets, values, width=width, label=matcher)

        plt.title(f"Known pairs runtime ({'extract' if extract else 'no extract'})")
        plt.xlabel("test suite")
        plt.ylabel("seconds")
        plt.xticks(x, cases, rotation=45, ha="right")
        plt.legend()
        plt.grid(axis="y", alpha=0.3)
        suffix = "extract" if extract else "no_extract"
        save_fig(out_dir / f"{path.stem}-known-pairs-{suffix}.png")

        # Mismatch/skip overview.
        labels = matchers
        skipped = []
        mismatches = []
        for matcher in matchers:
            skipped.append(sum(parse_int(r["skip_cases"]) for r in subrows if r["matcher"] == matcher))
            mismatches.append(sum(parse_int(r["mismatches"]) for r in subrows if r["matcher"] == matcher))

        plt.figure(figsize=(max(8, len(labels) * 1.2), 5))
        pos = list(range(len(labels)))
        plt.bar([p - 0.18 for p in pos], skipped, width=0.36, label="skipped")
        plt.bar([p + 0.18 for p in pos], mismatches, width=0.36, label="mismatches")
        plt.title(f"Known pairs skipped/mismatches ({suffix})")
        plt.xlabel("matcher")
        plt.ylabel("cases")
        plt.xticks(pos, labels, rotation=30, ha="right")
        plt.legend()
        plt.grid(axis="y", alpha=0.3)
        save_fig(out_dir / f"{path.stem}-known-pairs-status-{suffix}.png")


def plot_fuzzy(path: Path, rows: list[dict[str, str]], case_col: str, out_dir: Path) -> None:
    for extract, subrows in rows_by_extract(rows).items():
        if not subrows:
            continue

        by_matcher: dict[str, list[dict[str, str]]] = defaultdict(list)
        for row in subrows:
            by_matcher[row["matcher"]].append(row)

        plt.figure(figsize=(9, 6))
        for matcher in sorted(by_matcher.keys(), key=matcher_sort_key):
            points = []
            for row in by_matcher[matcher]:
                x = parse_int(row[case_col])
                y = parse_float(row["matcher_time"])
                if parse_int(row["run_cases"]) > 0:
                    points.append((x, y))
            points.sort()
            if points:
                plt.plot([p[0] for p in points], [p[1] for p in points], marker="o", label=matcher)

        libc_points_by_x: dict[int, list[float]] = defaultdict(list)
        for row in subrows:
            libc_points_by_x[parse_int(row[case_col])].append(parse_float(row["libc_time"]))
        libc_points = sorted((x, sum(v) / len(v)) for x, v in libc_points_by_x.items() if v)
        if libc_points:
            plt.plot([p[0] for p in libc_points], [p[1] for p in libc_points], marker="x", linestyle="--", label="LIBC")

        plt.title(f"Fuzzy runtime by input size ({'extract' if extract else 'no extract'})")
        plt.xlabel("max input length")
        plt.ylabel("seconds")
        plt.xscale("log", base=2)
        if any(parse_float(row["matcher_time"]) > 0 for row in subrows):
            plt.yscale("log")
        plt.legend()
        plt.grid(True, which="both", alpha=0.3)
        suffix = "extract" if extract else "no_extract"
        save_fig(out_dir / f"{path.stem}-fuzzy-runtime-{suffix}.png")

        plt.figure(figsize=(9, 6))
        for matcher in sorted(by_matcher.keys(), key=matcher_sort_key):
            points = []
            for row in by_matcher[matcher]:
                run_cases = parse_int(row["run_cases"])
                if run_cases <= 0:
                    continue
                x = parse_int(row[case_col])
                y = parse_float(row["matcher_time"]) / run_cases
                points.append((x, y))
            points.sort()
            if points:
                plt.plot([p[0] for p in points], [p[1] for p in points], marker="o", label=matcher)

        plt.title(f"Fuzzy time per executed case ({suffix})")
        plt.xlabel("max input length")
        plt.ylabel("seconds / case")
        plt.xscale("log", base=2)
        plt.yscale("log")
        plt.legend()
        plt.grid(True, which="both", alpha=0.3)
        save_fig(out_dir / f"{path.stem}-fuzzy-per-case-{suffix}.png")

        labels = sorted(by_matcher.keys(), key=matcher_sort_key)
        mismatches = [sum(parse_int(r["mismatches"]) for r in by_matcher[m]) for m in labels]
        skipped = [sum(parse_int(r["skip_cases"]) for r in by_matcher[m]) for m in labels]
        plt.figure(figsize=(max(8, len(labels) * 1.2), 5))
        pos = list(range(len(labels)))
        plt.bar([p - 0.18 for p in pos], skipped, width=0.36, label="skipped")
        plt.bar([p + 0.18 for p in pos], mismatches, width=0.36, label="mismatches")
        plt.title(f"Fuzzy skipped/mismatches ({suffix})")
        plt.xlabel("matcher")
        plt.ylabel("cases")
        plt.xticks(pos, labels, rotation=30, ha="right")
        plt.legend()
        plt.grid(axis="y", alpha=0.3)
        save_fig(out_dir / f"{path.stem}-fuzzy-status-{suffix}.png")


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
        plt.figure(figsize=(max(10, len(files) * 0.55), 6))
        for mi, matcher in enumerate(matchers):
            values = []
            for file_name in files:
                row = grouped[file_name].get(matcher)
                if row is None or parse_int(row["run_cases"]) == 0:
                    values.append(0.0)
                else:
                    values.append(parse_float(row["matcher_time"]))
            offsets = [v + (mi - (len(matchers) - 1) / 2) * width for v in x]
            plt.bar(offsets, values, width=width, label=matcher)

        plt.title(f"File-input runtime ({'extract' if extract else 'no extract'})")
        plt.xlabel("input file")
        plt.ylabel("seconds")
        plt.xticks(x, files, rotation=45, ha="right")
        plt.legend()
        plt.grid(axis="y", alpha=0.3)
        suffix = "extract" if extract else "no_extract"
        save_fig(out_dir / f"{path.stem}-files-runtime-{suffix}.png")

        plt.figure(figsize=(max(10, len(files) * 0.55), 6))
        for matcher in matchers:
            points = []
            for idx, file_name in enumerate(files):
                row = grouped[file_name].get(matcher)
                if row is None:
                    continue
                run_cases = parse_int(row["run_cases"])
                if run_cases > 0:
                    points.append((idx, parse_float(row["matcher_time"]) / run_cases))
            if points:
                plt.plot([p[0] for p in points], [p[1] for p in points], marker="o", label=matcher)

        plt.title(f"File-input time per executed regex ({suffix})")
        plt.xlabel("input file")
        plt.ylabel("seconds / regex")
        plt.xticks(x, files, rotation=45, ha="right")
        plt.yscale("log")
        plt.legend()
        plt.grid(True, which="both", alpha=0.3)
        save_fig(out_dir / f"{path.stem}-files-per-regex-{suffix}.png")

        labels = matchers
        skipped = [sum(parse_int(r["skip_cases"]) for r in subrows if r["matcher"] == m) for m in labels]
        mismatches = [sum(parse_int(r["mismatches"]) for r in subrows if r["matcher"] == m) for m in labels]
        plt.figure(figsize=(max(8, len(labels) * 1.2), 5))
        pos = list(range(len(labels)))
        plt.bar([p - 0.18 for p in pos], skipped, width=0.36, label="skipped")
        plt.bar([p + 0.18 for p in pos], mismatches, width=0.36, label="mismatches")
        plt.title(f"File-input skipped/mismatches ({suffix})")
        plt.xlabel("matcher")
        plt.ylabel("cases")
        plt.xticks(pos, labels, rotation=30, ha="right")
        plt.legend()
        plt.grid(axis="y", alpha=0.3)
        save_fig(out_dir / f"{path.stem}-files-status-{suffix}.png")


def infer_kind(path: Path, rows: list[dict[str, str]]) -> str:
    stem = path.stem.lower()
    suites = {row.get("suite", "").lower() for row in rows}

    if "known" in stem or any("known" in s for s in suites):
        return "known"
    if "fuzzy" in stem or any(s.startswith("fuzzy") for s in suites):
        return "fuzzy"
    if "file" in stem or "files" in stem or any("file" in s for s in suites):
        return "files"
    return "generic"


def plot_generic(path: Path, rows: list[dict[str, str]], case_col: str, out_dir: Path) -> None:
    grouped: dict[str, float] = defaultdict(float)
    for row in rows:
        grouped[row["matcher"]] += parse_float(row["matcher_time"])

    labels = sorted(grouped.keys(), key=matcher_sort_key)
    values = [grouped[k] for k in labels]
    plt.figure(figsize=(max(8, len(labels) * 1.2), 5))
    plt.bar(labels, values)
    plt.title(f"Runtime summary: {path.name}")
    plt.xlabel("matcher")
    plt.ylabel("seconds")
    plt.xticks(rotation=30, ha="right")
    plt.grid(axis="y", alpha=0.3)
    save_fig(out_dir / f"{path.stem}-runtime-summary.png")


def plot_csv(path: Path, out_dir: Path | None) -> None:
    rows, case_col = read_rows(path)
    if not rows:
        print(f"warning: {path} has no rows", file=sys.stderr)
        return

    target_dir = out_dir if out_dir is not None else path.parent
    kind = infer_kind(path, rows)

    if kind == "known":
        plot_known_pairs(path, rows, case_col, target_dir)
    elif kind == "fuzzy":
        plot_fuzzy(path, rows, case_col, target_dir)
    elif kind == "files":
        plot_files(path, rows, case_col, target_dir)
    else:
        plot_generic(path, rows, case_col, target_dir)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Plot Meta regex benchmark CSV files.")
    parser.add_argument("paths", nargs="+", type=Path, help="CSV files or directories containing CSV files")
    parser.add_argument("--out-dir", type=Path, default=None, help="directory for generated PNG files")
    args = parser.parse_args(argv)

    csvs = discover_csvs(args.paths)
    if not csvs:
        print("no CSV files found", file=sys.stderr)
        return 1

    for path in csvs:
        try:
            plot_csv(path, args.out_dir)
        except Exception as exc:
            print(f"error plotting {path}: {exc}", file=sys.stderr)
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
