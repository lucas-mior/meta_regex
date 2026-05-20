#!/usr/bin/env python3

import argparse
import csv
import hashlib
import json
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt

# Canonical, global colors. These are intentionally not derived from the
# subset of series present in a particular CSV/plot, so the same engine/matcher
# keeps the same color everywhere.
CANONICAL_SERIES_COLORS = {
    "LIBC": "#000000",
    "META_DISPATCH": "#0072B2",  # blue
    "BTNFA": "#E69F00",          # orange
    "TNFA": "#009E73",           # green
    "TDFA": "#D55E00",           # vermillion
    "LAZY_DFA": "#CC79A7",       # reddish purple
    "STATIC_DFA": "#56B4E9",     # sky blue
}

CANONICAL_SERIES_ORDER = [
    "LIBC",
    "META_DISPATCH",
    "BTNFA",
    "TNFA",
    "TDFA",
    "LAZY_DFA",
    "STATIC_DFA",
]

# Fallback palette for future matcher names. The choice is deterministic by
# series name, not by plotting order.
FALLBACK_COLORS = [
    "#332288", "#88CCEE", "#44AA99", "#117733", "#999933",
    "#DDCC77", "#CC6677", "#882255", "#AA4499", "#DDDDDD",
]

SERIES_ALIASES = {
    "LIBC": "LIBC",
    "REGEX_H": "LIBC",
    "REGEXEC": "LIBC",
    "META": "META_DISPATCH",
    "META_DISPATCH": "META_DISPATCH",
    "DISPATCH": "META_DISPATCH",
    "DISPATCHER": "META_DISPATCH",
    "MIXED": "META_DISPATCH",
    "BTNFA": "BTNFA",
    "MATCHER_BTNFA": "BTNFA",
    "TNFA": "TNFA",
    "MATCHER_TNFA": "TNFA",
    "TDFA": "TDFA",
    "MATCHER_TDFA": "TDFA",
    "LAZY_DFA": "LAZY_DFA",
    "MATCHER_LAZY_DFA": "LAZY_DFA",
    "STATIC_DFA": "STATIC_DFA",
    "MATCHER_STATIC_DFA": "STATIC_DFA",
}


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
    p.add_argument(
        "--log-y",
        action="store_true",
        help="Use logarithmic y-axis.",
    )
    p.add_argument(
        "--log-x",
        action="store_true",
        help="Use logarithmic x-axis. Useful because input buckets double in size.",
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


def clean_token(value):
    return str(value or "").strip().strip('"').strip("'")


def canonical_series_name(value):
    token = clean_token(value).upper().replace("-", "_").replace(" ", "_")
    return SERIES_ALIASES.get(token, token)


def is_libc_comparison_block(block):
    block = clean_token(block)
    return block == "libc_vs_dispatch" or block == "libc_vs_dispatch_pairwise"


def row_series_name(row):
    block = clean_token(row.get("block"))

    if is_libc_comparison_block(block):
        raw = row.get("engine") or row.get("matcher") or row.get("selected_matcher")
    else:
        raw = row.get("matcher") or row.get("engine") or row.get("selected_matcher")

    return canonical_series_name(raw)


def series_sort_key(name):
    try:
        return (0, CANONICAL_SERIES_ORDER.index(name))
    except ValueError:
        return (1, name)


def color_for_series(name):
    name = canonical_series_name(name)
    if name in CANONICAL_SERIES_COLORS:
        return CANONICAL_SERIES_COLORS[name]

    digest = hashlib.sha256(name.encode("utf-8")).digest()
    idx = int.from_bytes(digest[:4], "little") % len(FALLBACK_COLORS)
    return FALLBACK_COLORS[idx]


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


def csv_name_for_metadata(row):
    return {
        "block": clean_token(row.get("block")),
        "variant": clean_token(row.get("variant")),
        "feature_class": clean_token(row.get("feature_class")),
        "regex_length_class": clean_token(row.get("regex_length_class")),
        "regex_max_len": to_int(row, "regex_max_len"),
    }


def plot_csv(path, out_dir, metric, log_x=False, log_y=False):
    rows = read_rows(path)
    if not rows:
        return []

    outputs = []
    plot_groups = defaultdict(list)

    for r in rows:
        block = clean_token(r.get("block"))
        variant = clean_token(r.get("variant"))
        if is_libc_comparison_block(block):
            group_key = (block, "extract_vs_no_extract")
        else:
            group_key = (block, variant)
        plot_groups[group_key].append(r)

    for (block, variant_label), selected in plot_groups.items():
        if not selected:
            continue

        by_series = defaultdict(list)
        for row in selected:
            name = row_series_name(row)
            variant = clean_token(row.get("variant"))
            if not name:
                continue
            run_pairs = to_int(row, "run_pair_count")
            if run_pairs <= 0:
                continue
            x = to_int(row, "input_max_len")
            if x <= 0:
                continue
            y = to_float(row, metric)
            if y < 0:
                continue
            by_series[(name, variant)].append((x, y, row))

        if not by_series:
            continue

        fig, ax = plt.subplots(figsize=(10.5, 6.2))
        plotted = []
        used_colors = {}

        variant_order = {"no_extract": 0, "extract": 1}
        for (name, variant) in sorted(
            by_series.keys(),
            key=lambda k: (series_sort_key(k[0]), variant_order.get(k[1], 2), k[1]),
        ):
            points = by_series[(name, variant)]
            points.sort(key=lambda t: t[0])
            xs = [p[0] for p in points]
            ys = [p[1] for p in points]
            color = color_for_series(name)
            used_colors[name] = color

            linestyle = "--" if variant == "no_extract" else "-"
            label = f"{name} ({variant})" if variant_label == "extract_vs_no_extract" else name

            ax.plot(
                xs,
                ys,
                marker="o",
                linestyle=linestyle,
                linewidth=2.0,
                markersize=4.5,
                color=color,
                label=label,
            )
            plotted.append((name, variant))

        first = selected[0]
        info = csv_name_for_metadata(first)
        info["variant"] = variant_label
        ylabel = "seconds" if metric == "seconds" else "ns / match"

        ax.set_title(
            f"{info['block']} | {variant_label} | {info['feature_class']} | "
            f"regex <= {info['regex_max_len']}"
        )
        ax.set_xlabel("max input length")
        ax.set_ylabel(ylabel)
        ax.grid(True, alpha=0.3)
        ax.set_xticks(sorted({x for points in by_series.values() for x, _, _ in points}))

        if log_x:
            ax.set_xscale("log", base=2)
        if log_y:
            ax.set_yscale("log")

        if plotted:
            ax.legend()

        stem = f"{path.stem}-{variant_label}-{metric}"
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
            "metric": metric,
            "log_x": bool(log_x),
            "log_y": bool(log_y),
            **info,
            "series": [
                {"name": name, "variant": variant, "color": used_colors[name]}
                for name, variant in plotted
            ],
            "rows": [
                {
                    "series": row_series_name(r),
                    "variant": clean_token(r.get("variant")),
                    "input_max_len": to_int(r, "input_max_len"),
                    metric: to_float(r, metric),
                    "run_pair_count": to_int(r, "run_pair_count"),
                }
                for r in selected
                if (row_series_name(r), clean_token(r.get("variant"))) in plotted and to_int(r, "run_pair_count") > 0
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
        made.extend(plot_csv(path, args.out_dir, args.metric, args.log_x, args.log_y))

    for p in made:
        print(p)


if __name__ == "__main__":
    main()
