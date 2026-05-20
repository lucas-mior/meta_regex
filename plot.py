#!/usr/bin/env python3
import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt

LENGTH_MAX = {
    "1_16": 16,
    "17_32": 32,
    "33_64": 64,
    "65_128": 128,
}

SERIES_COLORS = {
    "LIBC": "black",
    "META_DISPATCH": "tab:blue",
    "BTNFA": "tab:orange",
    "TNFA": "tab:green",
    "TDFA": "tab:red",
    "LAZY_DFA": "tab:purple",
    "STATIC_DFA": "tab:brown",
}

SERIES_ORDER = [
    "LIBC",
    "META_DISPATCH",
    "BTNFA",
    "TNFA",
    "TDFA",
    "LAZY_DFA",
    "STATIC_DFA",
]


def discover_csvs(paths):
    csvs = []
    for raw in paths:
        path = Path(raw)
        if path.is_dir():
            csvs.extend(sorted(path.glob("*-libc_vs_meta-*.csv")))
            csvs.extend(sorted(path.glob("*-meta_matchers-*.csv")))
        else:
            csvs.append(path)
    return sorted(dict.fromkeys(csvs))


def read_rows(path):
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def row_series(row):
    block = row.get("block", "")
    if block == "libc_vs_dispatch":
        engine = row.get("engine", "")
        if engine == "LIBC":
            return "LIBC"
        return "META_DISPATCH"
    if block == "meta_matchers":
        return row.get("matcher", "")
    return row.get("engine") or row.get("matcher") or "unknown"


def sort_series(names):
    order = {name: i for i, name in enumerate(SERIES_ORDER)}
    return sorted(names, key=lambda name: (order.get(name, 1000), name))


def aggregate(rows, variant, metric):
    grouped = defaultdict(lambda: defaultdict(lambda: {
        "seconds": 0.0,
        "iterations": 0,
        "matches": 0,
        "rows": 0,
    }))

    for row in rows:
        if row.get("variant") != variant:
            continue

        length_class = row.get("length_class", "")
        if length_class not in LENGTH_MAX:
            continue

        series = row_series(row)
        if not series:
            continue

        x = LENGTH_MAX[length_class]
        seconds = float(row.get("seconds", "0") or 0)
        iterations = int(row.get("iterations", "0") or 0)
        matches = int(row.get("matches", "0") or 0)

        cell = grouped[series][x]
        cell["seconds"] += seconds
        cell["iterations"] += iterations
        cell["matches"] += matches
        cell["rows"] += 1

    plotted = {}
    for series, by_x in grouped.items():
        points = []
        for x, cell in sorted(by_x.items()):
            if metric == "seconds":
                y = cell["seconds"]
            elif metric == "ns_per_match":
                if cell["iterations"] <= 0:
                    continue
                y = (cell["seconds"] * 1_000_000_000.0) / cell["iterations"]
            else:
                raise ValueError(metric)

            points.append({
                "x": x,
                "y": y,
                "seconds": cell["seconds"],
                "iterations": cell["iterations"],
                "matches": cell["matches"],
                "rows": cell["rows"],
            })

        if points:
            plotted[series] = points

    return plotted


def title_from_rows(path, rows, variant):
    block = rows[0].get("block", path.stem) if rows else path.stem
    feature = rows[0].get("feature_class", "") if rows else ""
    return f"{block} / {feature} / {variant}"


def ylabel(metric):
    if metric == "seconds":
        return "Time taken (seconds)"
    if metric == "ns_per_match":
        return "Time taken per match (ns)"
    return metric


def plot_one(path, rows, variant, metric, out_dir):
    plotted = aggregate(rows, variant, metric)
    if not plotted:
        return None

    fig, ax = plt.subplots(figsize=(10, 6))

    for series in sort_series(plotted.keys()):
        points = plotted[series]
        xs = [p["x"] for p in points]
        ys = [p["y"] for p in points]
        ax.plot(
            xs,
            ys,
            marker="o",
            linewidth=2,
            label=series,
            color=SERIES_COLORS.get(series),
        )

    ax.set_title(title_from_rows(path, rows, variant))
    ax.set_xlabel("Max regex length")
    ax.set_ylabel(ylabel(metric))
    ax.set_xticks([16, 32, 64, 128])
    ax.grid(True, axis="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()

    out_dir.mkdir(parents=True, exist_ok=True)
    out_png = out_dir / f"{path.stem}-{variant}-{metric}.png"
    out_json = out_dir / f"{path.stem}-{variant}-{metric}.json"

    metadata = {
        "source_csv": str(path),
        "variant": variant,
        "metric": metric,
        "plotted_series": sort_series(plotted.keys()),
        "points": plotted,
    }

    fig.savefig(out_png, dpi=160, metadata={
        "source_csv": str(path),
        "variant": variant,
        "metric": metric,
        "plotted_series": ",".join(metadata["plotted_series"]),
    })
    plt.close(fig)

    out_json.write_text(json.dumps(metadata, indent=2, sort_keys=True))
    return out_png


def main():
    parser = argparse.ArgumentParser(
        description="Plot Meta regex benchmark CSV files."
    )
    parser.add_argument(
        "paths",
        nargs="+",
        help="CSV files or directories containing benchmark CSV files.",
    )
    parser.add_argument(
        "--out-dir",
        default=None,
        help="Output directory. Defaults to <first input directory>/plots or CSV parent/plots.",
    )
    parser.add_argument(
        "--metric",
        choices=["seconds", "ns_per_match"],
        default="seconds",
        help="Y-axis metric. Default: seconds.",
    )
    args = parser.parse_args()

    csvs = discover_csvs(args.paths)
    if not csvs:
        raise SystemExit("No benchmark CSV files found.")

    if args.out_dir is not None:
        out_dir = Path(args.out_dir)
    else:
        first = Path(args.paths[0])
        out_dir = (first if first.is_dir() else first.parent) / "plots"

    written = []
    for csv_path in csvs:
        rows = read_rows(csv_path)
        variants = sorted({row.get("variant", "") for row in rows if row.get("variant")})
        for variant in variants:
            out = plot_one(csv_path, rows, variant, args.metric, out_dir)
            if out is not None:
                written.append(out)

    for path in written:
        print(path)


if __name__ == "__main__":
    main()
