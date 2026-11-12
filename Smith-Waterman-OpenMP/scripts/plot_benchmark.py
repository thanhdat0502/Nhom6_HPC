"""Validate measured CSV independently, then render report-ready PNG charts.

Core validation uses only the standard library. Matplotlib is imported only
for plotting; it is never required by the C++ implementation or CTest.
"""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import statistics
from collections import defaultdict


def require(condition, message):
    if not condition:
        raise ValueError(message)


def near(actual, expected, label):
    require(math.isfinite(actual) and math.isclose(actual, expected, rel_tol=1e-10, abs_tol=1e-12),
            f"{label}: got {actual}, expected {expected}")


def read_csv(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    require(rows and all(None not in r and all(v is not None for v in r.values()) for r in rows), f"Malformed CSV: {path}")
    return rows


def fasta(path):
    records = []
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        if not line:
            continue
        if line.startswith(">"):
            records.append([line[1:].split()[0], ""])
        else:
            require(records and set(line) <= set("ACGT"), f"Invalid FASTA: {path}")
            records[-1][1] += line
    return records


def validate(directory):
    meta = json.loads((directory / "run_metadata.json").read_text(encoding="utf-8"))
    require(meta["status"] == "complete", "Benchmark run did not finish successfully")
    require(meta["build_type"] == "Release", "Debug/non-Release results cannot be used for performance plots")
    raw = read_csv(directory / "raw_benchmark.csv")
    summary = read_csv(directory / "summary_benchmark.csv")
    groups, serial, counts, hashes = defaultdict(list), defaultdict(list), defaultdict(int), {}
    run_ids = defaultdict(set)
    config_keys = {(c["mode"], c["schedule"], c["requested_threads"]) for c in meta["configurations"]}
    datasets = {d["id"]: d for d in meta["datasets"]}
    checksum_by_dataset, order_by_dataset = defaultdict(set), defaultdict(set)
    for dataset_id, d in datasets.items():
        query_path, refs_path = directory / d["query_fasta"], directory / d["references_fasta"]
        query, refs = fasta(query_path), fasta(refs_path)
        require(len(query) == 1 and len(query[0][1]) == d["query_length"], f"Query metadata: {dataset_id}")
        require(len(refs) == d["reference_count"], f"Reference count: {dataset_id}")
        require([len(seq) for _, seq in refs] == d["reference_lengths"], f"Reference lengths: {dataset_id}")
        require(d["dp_cells"] == len(query[0][1]) * sum(len(seq) for _, seq in refs), f"DP work: {dataset_id}")
        for path in [query_path, refs_path]:
            hashes[path.relative_to(directory).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    for r in raw:
        dataset_id = r["dataset_id"]
        require(dataset_id in datasets, "Unknown dataset in raw CSV")
        d = datasets[dataset_id]
        requested, actual = int(r["requested_threads"]), int(r["actual_threads"])
        require(r["build_type"] == "Release" and r["verified"] == "1", "Unverified/non-Release sample")
        require(actual > 0 and requested > 0, "Invalid thread count")
        require((r["mode"], r["schedule"], requested) in config_keys, "Unknown configuration")
        require(int(r["dp_cells"]) == d["dp_cells"], "Inconsistent raw DP work")
        require(int(r["query_length"]) == d["query_length"] and int(r["reference_count"]) == d["reference_count"], "Inconsistent dimensions")
        require(r["seed"] == d["seed"] and r["distribution"] == d["distribution"], "Inconsistent dataset labels")
        require(int(r["reference_min_length"]) == min(d["reference_lengths"]) and
                int(r["reference_max_length"]) == max(d["reference_lengths"]), "Inconsistent length range")
        seconds = float(r["execution_seconds"])
        require(math.isfinite(seconds) and seconds > 0, "Invalid execution time")
        require(r["phase"] in {"warmup", "measure"}, "Unknown sample phase")
        limit = meta["warmups"] if r["phase"] == "warmup" else meta["runs"]
        require(1 <= int(r["run_index"]) <= limit, "Invalid run index")
        config_phase = dataset_id, r["mode"], r["schedule"], requested, r["phase"]
        require(int(r["run_index"]) not in run_ids[config_phase], "Duplicate run index")
        run_ids[config_phase].add(int(r["run_index"]))
        order = int(r["order_index"])
        require(order not in order_by_dataset[dataset_id], "Duplicate sample order")
        order_by_dataset[dataset_id].add(order)
        counts[config_phase] += 1
        checksum_by_dataset[dataset_id].add(r["score_checksum"])
        if r["phase"] == "measure":
            key = dataset_id, r["mode"], r["schedule"], requested, actual
            groups[key].append(seconds)
            if r["mode"] == "serial":
                require(actual == requested == 1, "Serial thread metadata")
                serial[dataset_id].append(seconds)
    for dataset_id in datasets:
        expected_samples = len(config_keys) * (meta["runs"] + meta["warmups"])
        require(order_by_dataset[dataset_id] == set(range(1, expected_samples + 1)), "Missing/reordered raw sample IDs")
        require(len(checksum_by_dataset[dataset_id]) == 1, "Score checksums differ across configurations")
        for mode, schedule, requested in config_keys:
            for phase, count in [("warmup", meta["warmups"]), ("measure", meta["runs"])]:
                require(counts[(dataset_id, mode, schedule, requested, phase)] == count, "Missing repetition")
    seen = set()
    for r in summary:
        key = r["dataset_id"], r["mode"], r["schedule"], int(r["requested_threads"]), int(r["actual_threads"])
        require(key in groups and key not in seen, "Missing/duplicate summary group")
        require(r["build_type"] == "Release" and int(r["dp_cells"]) == datasets[key[0]]["dp_cells"], "Invalid summary metadata")
        seen.add(key)
        values = groups[key]
        median = statistics.median(values)
        expected = {
            "median_seconds": median, "mean_seconds": statistics.mean(values),
            "min_seconds": min(values), "max_seconds": max(values),
            "population_stddev_seconds": statistics.pstdev(values),
            "speedup": statistics.median(serial[key[0]]) / median,
            "gcups": datasets[key[0]]["dp_cells"] / (median * 1e9),
        }
        expected["efficiency"] = expected["speedup"] / key[4]
        expected["efficiency_percent"] = expected["efficiency"] * 100
        for field, value in expected.items():
            near(float(r[field]), value, f"{key}: {field}")
        require(int(r["samples"]) == len(values), "Incorrect sample count")
        require(int(r["complete_samples"]) == int(len(values) == meta["runs"]), "Incorrect completeness flag")
        require(int(r["thread_mismatch"]) == int(key[3] != key[4]), "Incorrect mismatch flag")
    require(seen == set(groups), "Some measured groups were not summarized")
    report = {"datasets": len(datasets), "warmups": meta["warmups"], "runs": meta["runs"],
              "raw_rows": len(raw), "measured_rows": sum(len(v) for v in groups.values()),
              "summary_rows": len(summary), "all_formulas_verified": True, "all_repetitions_present": True,
              "dataset_sha256": hashes,
              "mismatched_summary_groups": sum(int(r["thread_mismatch"]) for r in summary),
              "incomplete_summary_groups": sum(r["complete_samples"] != "1" for r in summary)}
    return summary, report


def plot(directory, rows, report, output, replace=False):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    require(not output.exists() or replace, f"Refusing to overwrite figures: {output}; use --replace-figures to rerender derived files")
    output.mkdir(parents=True, exist_ok=replace)
    # Preserve all actual-team groups in CSV. Only complete, matching teams are
    # plotted on the requested-thread axis; no silently merged reduced teams.
    usable = [r for r in rows if r["complete_samples"] == "1" and r["thread_mismatch"] == "0"]
    ids = list(dict.fromkeys(r["dataset_id"] for r in rows))
    colors = {"static": "#2563eb", "dynamic": "#e67e22"}
    plt.rcParams.update({"font.size": 10, "axes.grid": True, "grid.alpha": .22, "savefig.dpi": 200})
    for field, title, ylabel, filename in [
        ("median_seconds", "Execution time", "Median compute time (ms)", "execution_time.png"),
        ("speedup", "Parallel speedup", "Speedup (Tserial / Tparallel)", "speedup.png"),
        ("efficiency_percent", "Parallel efficiency", "Efficiency (%)", "efficiency.png"),
    ]:
        fig, axes = plt.subplots(math.ceil(len(ids) / 2), 2, figsize=(13, 3.4 * math.ceil(len(ids) / 2)), squeeze=False)
        for ax, dataset_id in zip(axes.flat, ids):
            subset = [r for r in usable if r["dataset_id"] == dataset_id]
            baseline = next(r for r in subset if r["mode"] == "serial")
            for schedule in ["static", "dynamic"]:
                line = sorted((r for r in subset if r["schedule"] == schedule), key=lambda r: int(r["actual_threads"]))
                if not line:
                    continue
                factor = 1000 if field == "median_seconds" else 1
                x = [int(r["actual_threads"]) for r in line]
                y = [float(r[field]) * factor for r in line]
                if field == "median_seconds":
                    errors = [[max(0, float(r["median_seconds"]) - float(r["min_seconds"])) * factor for r in line],
                              [max(0, float(r["max_seconds"]) - float(r["median_seconds"])) * factor for r in line]]
                    ax.errorbar(x, y, yerr=errors, marker="o", capsize=2, label=schedule.title(), color=colors[schedule])
                else:
                    ax.plot(x, y, marker="o", label=schedule.title(), color=colors[schedule])
            reference = float(baseline["median_seconds"]) * 1000 if field == "median_seconds" else (1 if field == "speedup" else 100)
            ax.axhline(reference, color="#64748b", linestyle="--", label="Serial" if field == "median_seconds" else "Reference")
            ax.set(title=dataset_id, xlabel="Actual OpenMP team size", ylabel=ylabel)
            ax.set_xticks(sorted({int(r["actual_threads"]) for r in subset if r["mode"] == "parallel"}))
            ax.legend(fontsize=8)
        for ax in list(axes.flat)[len(ids):]:
            ax.set_visible(False)
        detail = "median; bars=min/max" if field == "median_seconds" else "median"
        fig.suptitle(title + f" — {report['runs']} measured runs/configuration ({detail})", fontsize=14)
        fig.tight_layout(rect=(0, 0, 1, .96))
        fig.savefig(output / filename)
        plt.close(fig)

    # Static/dynamic ratio: >1 favors dynamic, <1 favors static.
    distributions = list(dict.fromkeys(r["distribution"] for r in usable))
    fig, axes = plt.subplots(1, len(distributions), figsize=(6.5 * len(distributions), 4.8), squeeze=False)
    levels = sorted({int(r["actual_threads"]) for r in usable if r["mode"] == "parallel"})
    for ax, distribution in zip(axes.flat, distributions):
        dataset_ids = [i for i in ids if any(r["dataset_id"] == i and r["distribution"] == distribution for r in usable)]
        have_comparison = False
        for p in levels:
            ratios = []
            for dataset_id in dataset_ids:
                pair = {r["schedule"]: float(r["median_seconds"]) for r in usable if r["dataset_id"] == dataset_id
                        and r["mode"] == "parallel" and int(r["actual_threads"]) == p}
                ratios.append(pair["static"] / pair["dynamic"] if len(pair) == 2 else math.nan)
            if any(math.isfinite(r) for r in ratios):
                have_comparison = True
                ax.plot([i.split("_")[0] for i in dataset_ids], ratios, marker="o", label=f"{p} thread" + ("s" if p != 1 else ""))
        ax.axhline(1, color="#64748b", linestyle="--")
        ax.set(title=distribution.title(), xlabel="Dataset scale", ylabel="Static time / dynamic time")
        if have_comparison:
            ax.legend(fontsize=8)
        else:
            ax.text(.5, .5, "Both schedules are required for a comparison", ha="center", transform=ax.transAxes)
    fig.suptitle("Scheduling comparison (>1 favors dynamic)")
    fig.tight_layout()
    fig.savefig(output / "scheduling_comparison.png")
    plt.close(fig)

    fig, axes = plt.subplots(1, len(distributions), figsize=(6.5 * len(distributions), 4.8), squeeze=False)
    for ax, distribution in zip(axes.flat, distributions):
        selected = [r for r in usable if r["distribution"] == distribution]
        labels = [("serial", "none", 1)] + [("parallel", "dynamic", p) for p in levels]
        for mode, schedule, p in labels:
            line = sorted((r for r in selected if r["mode"] == mode and r["schedule"] == schedule and int(r["actual_threads"]) == p),
                          key=lambda r: int(r["dp_cells"]))
            if line:
                ax.plot([int(r["dp_cells"]) for r in line], [float(r["median_seconds"]) for r in line], marker="o",
                        label="Serial" if mode == "serial" else f"Dynamic {p} threads")
        ax.set(xscale="log", yscale="log", xlabel="DP cells per batch", ylabel="Median compute time (s)", title=distribution.title())
        if len({r["dp_cells"] for r in selected}) < 2:
            ax.text(.5, .02, "One workload size: scaling cannot be inferred", ha="center", transform=ax.transAxes, fontsize=8)
        ax.legend(fontsize=8)
    fig.suptitle("Runtime versus workload (log-log)")
    fig.tight_layout()
    fig.savefig(output / "size_scaling.png")
    plt.close(fig)
    (output / "validation.json").write_text(json.dumps(report, indent=2), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--validate-only", action="store_true")
    parser.add_argument("--figures", type=Path)
    parser.add_argument("--replace-figures", action="store_true", help="Intentionally rerender derived PNG/validation files; never modifies measured CSV")
    args = parser.parse_args()
    rows, report = validate(args.directory)
    print(json.dumps(report, indent=2))
    if not args.validate_only:
        plot(args.directory, rows, report, args.figures or args.directory / "figures", args.replace_figures)


if __name__ == "__main__":
    main()
