"""Validate and compare two saved runs without modifying either input directory."""

import argparse
import csv
import hashlib
import json
from pathlib import Path
import statistics

from plot_benchmark import read_csv, require, validate


def key(row):
    return (row["dataset_id"], row["mode"], row["schedule"],
            int(row["requested_threads"]), int(row["actual_threads"]))


def compare(original, repeated, output):
    old, old_validation = validate(original)
    new, new_validation = validate(repeated)
    require(old_validation["dataset_sha256"] == new_validation["dataset_sha256"], "Dataset bytes differ")
    old_meta, new_meta = [json.loads((d / "run_metadata.json").read_text(encoding="utf-8")) for d in (original, repeated)]
    for field in ("build_type", "compiler", "openmp_macro", "warmups", "runs", "order", "order_seed_recipe",
                  "environment", "configurations", "datasets"):
        require(old_meta[field] == new_meta[field], "Protocol differs: " + field)
    checksums = []
    for directory in (original, repeated):
        grouped = {}
        for row in read_csv(directory / "raw_benchmark.csv"):
            grouped.setdefault(row["dataset_id"], set()).add(row["score_checksum"])
        checksums.append(grouped)
    require(checksums[0] == checksums[1], "Cross-session score checksums differ")
    old_by_key, new_by_key = [{key(r): r for r in rows} for rows in (old, new)]
    require(old_by_key.keys() == new_by_key.keys(), "Measured configuration groups differ")
    comparisons = []
    for group, before in old_by_key.items():
        after = new_by_key[group]
        ratio = float(after["median_seconds"]) / float(before["median_seconds"])
        comparisons.append({"dataset_id": group[0], "mode": group[1], "schedule": group[2],
                            "requested_threads": group[3], "actual_threads": group[4],
                            "original_median_seconds": before["median_seconds"], "validation_median_seconds": after["median_seconds"],
                            "median_change_percent": (ratio - 1) * 100,
                            "original_speedup": before["speedup"], "validation_speedup": after["speedup"],
                            "observed_min_max_overlap": max(float(before["min_seconds"]), float(after["min_seconds"]))
                            <= min(float(before["max_seconds"]), float(after["max_seconds"]))})
    winners = []
    for dataset_id in dict.fromkeys(r["dataset_id"] for r in old):
        best_old, best_new = [min((r for r in rows if r["dataset_id"] == dataset_id and r["mode"] == "parallel"),
                                 key=lambda r: float(r["median_seconds"])) for rows in (old, new)]
        old_winner_repeated = new_by_key[key(best_old)]
        serial_old, serial_new = [next(r for r in rows if r["dataset_id"] == dataset_id and r["mode"] == "serial") for rows in (old, new)]
        winners.append({"dataset_id": dataset_id, "original_serial_ms": float(serial_old["median_seconds"]) * 1000,
                        "validation_serial_ms": float(serial_new["median_seconds"]) * 1000,
                        "original_winner": f"{best_old['schedule']}/{best_old['actual_threads']}",
                        "original_best_ms": float(best_old["median_seconds"]) * 1000,
                        "original_best_speedup": float(best_old["speedup"]),
                        "original_winner_validation_ms": float(old_winner_repeated["median_seconds"]) * 1000,
                        "original_winner_validation_speedup": float(old_winner_repeated["speedup"]),
                        "validation_winner": f"{best_new['schedule']}/{best_new['actual_threads']}",
                        "validation_best_ms": float(best_new["median_seconds"]) * 1000,
                        "validation_best_speedup": float(best_new["speedup"]),
                        "validation_best_efficiency_percent": float(best_new["efficiency_percent"]),
                        "same_winner": key(best_old) == key(best_new)})
    output.mkdir(parents=True, exist_ok=False)
    with (output / "comparison.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(comparisons[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(comparisons)
    changes = [r["median_change_percent"] for r in comparisons]
    result = {"original": original.as_posix(), "validation": repeated.as_posix(), "groups": len(comparisons),
              "dataset_bytes_identical": True, "protocol_identical": True, "cross_session_checksums_identical": True,
              "median_change_percent_min": min(changes), "median_change_percent_median": statistics.median(changes),
              "median_change_percent_max": max(changes), "same_winners": sum(w["same_winner"] for w in winners),
              "min_max_overlap_groups": sum(r["observed_min_max_overlap"] for r in comparisons),
              "interpretation": "Descriptive paired medians and observed ranges only; no confidence intervals, no optimization claim, no universal winner.",
              "winners": winners,
              "input_sha256": {str(d / f): hashlib.sha256((d / f).read_bytes()).hexdigest()
                               for d in (original, repeated) for f in ("raw_benchmark.csv", "summary_benchmark.csv", "run_metadata.json")}}
    (output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original", type=Path)
    parser.add_argument("repeated", type=Path)
    parser.add_argument("--output", type=Path, required=True, help="Must be a new directory")
    args = parser.parse_args()
    compare(args.original, args.repeated, args.output)


if __name__ == "__main__":
    main()
