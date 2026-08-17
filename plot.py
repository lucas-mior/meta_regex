#!/usr/bin/env python

import argparse
import csv
import hashlib
import json
import re
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt

# Canonical, global colors. These are intentionally not derived from the
# subset of series present in a particular CSV/plot, so the same engine/matcher
# keeps the same color everywhere.
CANONICAL_SERIES_COLORS = {
    "LIBC": "#000000",
    "META_DISPATCH": "#cc4400",
    "BTNFA": "#d69F00",
    "TNFA": "#bbbbbb",
    "TDFA": "#009e73",
    "LAZY_DFA": "#CC79A7",
    "STATIC_DFA": "#56B4E9",
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

VARIANT_ORDER = {"no_extract": 0, "extract": 1}
VARIANT_STYLES = {
    "no_extract": "--",
    "extract": "-",
}

# Slight transparency keeps overlapping extract/no_extract and matcher lines
# readable without changing the canonical colors themselves.
PLOT_ALPHA = 0.78


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
    p.add_argument(
        "--separate-variants",
        action="store_true",
        help="Write separate extract/no_extract plots instead of overlaying them.",
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


def slug(value):
    value = clean_token(value)
    value = value.replace("<=", "le")
    value = re.sub(r"[^A-Za-z0-9_.-]+", "_", value)
    value = value.strip("_")
    return value or "unknown"


def canonical_series_name(value):
    token = clean_token(value).upper().replace("-", "_").replace(" ", "_")
    return SERIES_ALIASES.get(token, token)


def first_present(row, names):
    for name in names:
        value = clean_token(row.get(name))
        if value:
            return value
    return ""


def row_series_name(row):
    raw_engine = clean_token(row.get("engine"))
    raw_matcher = clean_token(row.get("matcher"))
    raw_selected = clean_token(row.get("selected_matcher"))

    engine = canonical_series_name(raw_engine)
    matcher = canonical_series_name(raw_matcher)
    selected = canonical_series_name(raw_selected)

    dispatcher_tokens = {"META", "MIXED", "DISPATCH", "DISPATCHER", "META_DISPATCH"}

    if engine == "LIBC" or matcher == "LIBC" or selected == "LIBC":
        return "LIBC"

    # Individual matcher rows are emitted with engine="META" and
    # matcher="MATCHER_*".  Do not let the generic META engine alias swallow
    # those rows as META_DISPATCH.
    if raw_matcher and matcher not in dispatcher_tokens:
        return matcher

    if raw_selected and selected not in dispatcher_tokens:
        return selected

    if raw_engine and engine == "META_DISPATCH":
        return "META_DISPATCH"

    if engine:
        return engine

    return "UNKNOWN"


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


def max_from_length_class(value):
    value = clean_token(value)
    if not value:
        return 0
    pieces = re.findall(r"\d+", value)
    if not pieces:
        return 0
    return int(pieces[-1])


def row_array_name(row, path):
    value = first_present(row, [
        "array",
        "array_name",
        "regex_array",
        "regex_bucket",
    ])
    if value:
        return value

    # New benchmark files are normally named like
    # bench_regex_cases-YYYYmmdd-HHMMSS.csv.  Keep the array-ish prefix.
    stem = path.stem
    stem = re.sub(r"-\d{8}[-_]\d{6}$", "", stem)
    stem = re.sub(r"-\d{10,}$", "", stem)
    return stem


def row_regex_length_class(row):
    value = first_present(row, ["regex_length_class", "ops_length_class"])
    if value:
        return value
    max_len = to_int(row, "regex_max_len")
    return f"le_{max_len}" if max_len > 0 else "unknown_regex_ops"


def row_regex_max_len(row):
    max_len = to_int(row, "regex_max_len")
    if max_len > 0:
        return max_len
    return max_from_length_class(row_regex_length_class(row))


def row_input_length_class(row):
    value = first_present(row, ["input_length_class", "input_bucket"])
    return value or "unknown_input"


def row_input_max_len(row):
    x = to_int(row, "input_max_len")
    if x > 0:
        return x
    return max_from_length_class(row_input_length_class(row))


def row_feature_label(row):
    feature = clean_token(row.get("feature_class"))
    if feature:
        return feature
    is_backref = clean_token(row.get("is_backref"))
    if is_backref in {"1", "true", "yes"}:
        return "with_backreferences"
    if is_backref in {"0", "false", "no"}:
        return "no_backreferences"
    array = clean_token(row.get("array")) or clean_token(row.get("regex_bucket"))
    if "backref" in array.lower():
        return "with_backreferences"
    return "no_backreferences"


def row_test_name(row, path):
    value = first_present(row, ["test", "test_name", "name"])
    if value:
        return value
    return (
        f"{row_array_name(row, path)}_ops_{row_regex_length_class(row)}_"
        f"input_{row_input_length_class(row)}"
    )


def group_rows(rows, path, separate_variants):
    groups = defaultdict(list)
    for row in rows:
        variant = clean_token(row.get("variant")) or "no_extract"
        array_name = row_array_name(row, path)
        regex_class = row_regex_length_class(row)
        regex_max = row_regex_max_len(row)
        feature = row_feature_label(row)
        variant_key = variant if separate_variants else "extract_vs_no_extract"

        key = (array_name, regex_class, regex_max, feature, variant_key)
        groups[key].append(row)
    return groups


def averaged_points(rows, metric):
    by_series = defaultdict(list)
    accum = defaultdict(list)

    for row in rows:
        name = row_series_name(row)
        variant = clean_token(row.get("variant")) or "no_extract"
        run_pairs = to_int(row, "run_pair_count")
        if run_pairs <= 0:
            continue
        x = row_input_max_len(row)
        if x <= 0:
            continue
        y = to_float(row, metric)
        if y < 0:
            continue
        accum[(name, variant, x)].append((y, row))

    for (name, variant, x), values in accum.items():
        y = sum(v[0] for v in values) / float(len(values))
        by_series[(name, variant)].append((x, y, values[0][1]))

    return by_series


def plot_csv(path, out_dir, metric, log_x=False, log_y=False, separate_variants=False):
    rows = read_rows(path)
    if not rows:
        return []

    outputs = []
    plot_groups = group_rows(rows, path, separate_variants)

    for (array_name, regex_class, regex_max, feature, variant_label), selected in sorted(plot_groups.items()):
        if not selected:
            continue

        by_series = averaged_points(selected, metric)
        if not by_series:
            continue

        fig, ax = plt.subplots(figsize=(10.5, 6.2))
        plotted = []
        used_colors = {}

        for (name, variant) in sorted(
            by_series.keys(),
            key=lambda k: (series_sort_key(k[0]), VARIANT_ORDER.get(k[1], 2), k[1]),
        ):
            points = by_series[(name, variant)]
            points.sort(key=lambda t: t[0])
            xs = [p[0] for p in points]
            ys = [p[1] for p in points]
            color = color_for_series(name)
            used_colors[name] = color

            linestyle = VARIANT_STYLES.get(variant, "-")
            label = name
            if variant_label == "extract_vs_no_extract":
                label = f"{name} ({variant})"

            ax.plot(
                xs,
                ys,
                marker="o",
                linestyle=linestyle,
                linewidth=2.0,
                markersize=4.5,
                color=color,
                alpha=PLOT_ALPHA,
                label=label,
            )
            plotted.append((name, variant))

        ylabel = "seconds" if metric == "seconds" else "ns / match"
        title_variant = variant_label
        title = f"{array_name} | ops {regex_class}"
        if regex_max > 0:
            title += f" (<= {regex_max})"
        if feature:
            title += f" | {feature}"
        title += f" | {title_variant}"

        ax.set_title(title)
        ax.set_xlabel("max input length")
        ax.set_ylabel(ylabel)
        ax.grid(True, alpha=0.3)
        xticks = sorted({x for points in by_series.values() for x, _, _ in points})
        if xticks:
            ax.set_xticks(xticks)

        if log_x:
            ax.set_xscale("log", base=2)
        if log_y:
            ax.set_yscale("log")

        if plotted:
            ax.legend()

        target_dir = Path(out_dir) if out_dir else path.parent
        target_dir.mkdir(parents=True, exist_ok=True)
        stem = (
            f"{path.stem}-{slug(array_name)}-ops_{slug(regex_class)}-"
            f"{slug(variant_label)}-{metric}"
        )
        png = target_dir / f"{stem}.png"
        json_dir = target_dir / "json"
        json_dir.mkdir(parents=True, exist_ok=True)
        meta = json_dir / f"{stem}.json"

        fig.tight_layout()
        fig.savefig(png, dpi=160)
        plt.close(fig)

        metadata = {
            "source_csv": str(path),
            "plot": str(png),
            "metric": metric,
            "log_x": bool(log_x),
            "log_y": bool(log_y),
            "array": array_name,
            "regex_length_class": regex_class,
            "regex_max_len": regex_max,
            "feature_class": feature,
            "variant": variant_label,
            "series": [
                {
                    "name": name,
                    "variant": variant,
                    "color": used_colors[name],
                    "alpha": PLOT_ALPHA,
                }
                for name, variant in plotted
            ],
            "rows": [
                {
                    "test": row_test_name(r, path),
                    "series": row_series_name(r),
                    "variant": clean_token(r.get("variant")) or "no_extract",
                    "input_length_class": row_input_length_class(r),
                    "input_max_len": row_input_max_len(r),
                    metric: to_float(r, metric),
                    "run_pair_count": to_int(r, "run_pair_count"),
                }
                for r in selected
                if (row_series_name(r), clean_token(r.get("variant")) or "no_extract") in plotted
                and to_int(r, "run_pair_count") > 0
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
        made.extend(
            plot_csv(
                path,
                args.out_dir,
                args.metric,
                args.log_x,
                args.log_y,
                args.separate_variants,
            )
        )

    for p in made:
        print(p)


if __name__ == "__main__":
    main()
