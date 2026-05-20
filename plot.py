#!/usr/bin/env python3
import argparse
import csv
import json
from pathlib import Path
from collections import defaultdict

import matplotlib.pyplot as plt

SERIES_COLORS = {
    "LIBC": "#000000",
    "META_DISPATCH": "#1f77b4",
    "BTNFA": "#ff7f0e",
    "TNFA": "#2ca02c",
    "TDFA": "#d62728",
    "LAZY_DFA": "#9467bd",
    "STATIC_DFA": "#8c564b",
    "DISPATCH": "#1f77b4",
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


def parse_args():
    p = argparse.ArgumentParser(description="Plot Meta regex benchmark CSV files.")
    p.add_argument("paths", nargs="+", help="CSV files or directories containing CSV files")
    p.add_argument("--out-dir", default=None, help="Output directory for plots")
    p.add_argument(
        "--metric",
        choices=["seconds", "ns_per_match"],
        default="seconds",
        help="Y-axis metric",
    )
    return p.parse_args()


def collect_csvs(paths):
    result = []
    for raw in paths:
        p = Path(raw)
        if p.is_dir():
            result.extend(sorted(p.glob("*.csv")))
        elif p.suffix == ".csv":
            result.append(p)
    return sorted(set(result))


def read_rows(path):
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def series_name(row):
    block = row.get("block", "")
    if block == "libc_vs_dispatch":
        return row.get("engine", "")
    return row.get("matcher", "")


def to_float(row, key):
    try:
        return float(row[key])
    except Exception:
        return 0.0


def to_int(row, key):
    try:
        return int(float(row[key]))
    except Exception:
        return 0


def plot_csv(path, out_dir, metric):
    rows = read_rows(path)
    if not rows:
        return []

    outputs = []
    variants = sorted({r.get("variant", "") for r in rows if r.get("variant", "")})

    for variant in variants:
        selected = [r for r in rows if r.get("variant") == variant]
        if not selected:
            continue

        by_series = defaultdict(list)
        for row in selected:
            name = series_name(row)
            if not name:
                continue
            run_pairs = to_int(row, "run_pair_count")
            if run_pairs <= 0:
                continue
            x = to_int(row, "input_max_len")
            y = to_float(row, metric)
            by_series[name].append((x, y, row))

        if not by_series:
            continue

        fig, ax = plt.subplots(figsize=(10, 6))
        plotted = []
        for name in SERIES_ORDER + sorted(set(by_series) - set(SERIES_ORDER)):
            points = by_series.get(name)
            if not points:
                continue
            points.sort(key=lambda t: t[0])
            xs = [p[0] for p in points]
            ys = [p[1] for p in points]
            ax.plot(
                xs,
                ys,
                marker="o",
                linewidth=2,
                color=SERIES_COLORS.get(name),
                label=name,
            )
            plotted.append(name)

        first = selected[0]
        feature = first.get("feature_class", "")
        regex_len = first.get("regex_length_class", "")
        regex_max = first.get("regex_max_len", "")
        block = first.get("block", "")

        ylabel = "seconds" if metric == "seconds" else "ns / match"
        ax.set_title(f"{block} | {variant} | {feature} | regex <= {regex_max}")
        ax.set_xlabel("max input length")
        ax.set_ylabel(ylabel)
        ax.grid(True, alpha=0.3)
        ax.legend()

        stem = f"{path.stem}-{variant}-{metric}"
        target_dir = Path(out_dir) if out_dir else path.parent
        target_dir.mkdir(parents=True, exist_ok=True)
        png = target_dir / f"{stem}.png"
        meta = target_dir / f"{stem}.json"

        fig.tight_layout()
        fig.savefig(png, dpi=160)
        plt.close(fig)

        metadata = {
            "source_csv": str(path),
            "plot": str(png),
            "block": block,
            "variant": variant,
            "feature_class": feature,
            "regex_length_class": regex_len,
            "regex_max_len": regex_max,
            "metric": metric,
            "series": plotted,
            "rows": [
                {
                    "series": series_name(r),
                    "input_max_len": to_int(r, "input_max_len"),
                    metric: to_float(r, metric),
                    "run_pair_count": to_int(r, "run_pair_count"),
                }
                for r in selected
                if series_name(r) in plotted and to_int(r, "run_pair_count") > 0
            ],
        }
        meta.write_text(json.dumps(metadata, indent=2))
        outputs.append(png)

    return outputs


def main():
    args = parse_args()
    csvs = collect_csvs(args.paths)
    if not csvs:
        raise SystemExit("No CSV files found.")

    made = []
    for path in csvs:
        made.extend(plot_csv(path, args.out_dir, args.metric))

    for p in made:
        print(p)


if __name__ == "__main__":
    main()
