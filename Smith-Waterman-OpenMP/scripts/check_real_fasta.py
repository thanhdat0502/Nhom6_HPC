"""Fetch public NCBI FASTA, or verify saved inputs against the existing CLI.

Download and verification are separate commands; verification needs no network.
This is a supplementary correctness check, not the main performance benchmark.
"""

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time
import urllib.request


ACCESSIONS = ("NC_001422.1", "NC_001330.1")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def record(data, accession):
    lines = data.decode("ascii").splitlines()
    if not lines or not lines[0].startswith(">" + accession + " "):
        raise ValueError("Unexpected NCBI FASTA header: " + accession)
    if any(line.startswith(">") for line in lines[1:]):
        raise ValueError("Expected exactly one record: " + accession)
    sequence = "".join(lines[1:])
    if not sequence or set(sequence) - set("ACGT"):
        raise ValueError("Input is empty or contains unsupported bases; do not silently remove them")
    return lines[0][1:], sequence


def save_json(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def download(directory):
    directory.mkdir(parents=True, exist_ok=False)
    entries = []
    for accession in ACCESSIONS:
        url = ("https://eutils.ncbi.nlm.nih.gov/entrez/eutils/efetch.fcgi"
               f"?db=nuccore&id={accession}&rettype=fasta&retmode=text")
        request = urllib.request.Request(url, headers={"User-Agent": "Week3SmithWatermanAudit/1.0"})
        with urllib.request.urlopen(request, timeout=30) as response:
            data = response.read()
        header, sequence = record(data, accession)
        path = directory / (accession + ".fasta")
        path.write_bytes(data)
        entries.append({"accession_version": accession, "header": header,
                        "record_url": "https://www.ncbi.nlm.nih.gov/nuccore/" + accession,
                        "download_url": url, "downloaded_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                        "filename": path.name, "length": len(sequence), "alphabet": sorted(set(sequence)),
                        "bytes": len(data), "sha256": digest(path)})
        time.sleep(0.4)  # Respect the public E-utilities request rate.
    query = directory / "query.fasta"
    references = directory / "references.fasta"
    query.write_bytes((directory / entries[0]["filename"]).read_bytes())
    references.write_bytes(b"\n".join((directory / e["filename"]).read_bytes().rstrip(b"\r\n") for e in entries) + b"\n")
    provenance = {"purpose": "Supplementary score equivalence on public phage genomes; no biological accuracy claim",
                  "sources": entries,
                  "derivation": "Query is the first original FASTA byte for byte. References concatenate both complete original records; only record separators change. No bases changed or synthesized. First reference is a self-match control.",
                  "query_id": ACCESSIONS[0], "query_sha256": digest(query), "references_sha256": digest(references)}
    save_json(directory / "provenance.json", provenance)
    print(json.dumps(provenance, indent=2))


def verify(directory, cli):
    provenance = json.loads((directory / "provenance.json").read_text(encoding="utf-8"))
    sequences = {}
    for entry in provenance["sources"]:
        path = directory / entry["filename"]
        if digest(path) != entry["sha256"]:
            raise ValueError("Source hash mismatch: " + str(path))
        header, sequence = record(path.read_bytes(), entry["accession_version"])
        if header != entry["header"] or len(sequence) != entry["length"]:
            raise ValueError("Source metadata mismatch")
        sequences[entry["accession_version"]] = sequence
    for filename, key in [("query.fasta", "query_sha256"), ("references.fasta", "references_sha256")]:
        if digest(directory / filename) != provenance[key]:
            raise ValueError("Derived input hash mismatch: " + filename)
    if (directory / "verification.json").exists():
        raise FileExistsError("Refusing to overwrite a prior verification")
    base = [str(cli.resolve()), "search", "--query", str(directory / "query.fasta"),
            "--query-id", provenance["query_id"], "--references", str(directory / "references.fasta")]
    env = dict(os.environ, OMP_DYNAMIC="FALSE", OMP_WAIT_POLICY="PASSIVE")
    checks, baseline = [], None

    def run(name, command, expected_success):
        completed = subprocess.run(command, capture_output=True, text=True, env=env, check=False)
        (directory / (name + "_stdout.txt")).write_text(completed.stdout, encoding="utf-8")
        (directory / (name + "_stderr.txt")).write_text(completed.stderr, encoding="utf-8")
        if expected_success:
            if completed.returncode != 0:
                raise AssertionError(f"{name}: exit {completed.returncode}: {completed.stderr}")
        elif completed.returncode != 1 or completed.stdout or not completed.stderr.startswith("Error:"):
            raise AssertionError(f"{name}: malformed FASTA was not cleanly rejected")
        checks.append({"name": name, "command": command, "exit_code": completed.returncode,
                       "expected_rejection": not expected_success, "pass": True})
        return completed

    for mode, schedule, threads in [("serial", "dynamic", 1)] + [
            ("parallel", s, p) for p in (1, 2, 4) for s in ("static", "dynamic")]:
        name = f"{mode}_{schedule}_{threads}"
        completed = run(name, base + ["--mode", mode, "--schedule", schedule, "--threads", str(threads), "--stats"], True)
        if baseline is None:
            baseline = completed.stdout
        if completed.stdout != baseline:
            raise AssertionError("Serial/parallel score output differs")
        match = re.search(r"Actual threads:\s*(\d+)", completed.stderr)
        if not match or int(match[1]) != threads:
            raise AssertionError("Actual threads did not match requested threads")
        checks[-1]["actual_threads"] = int(match[1])
    scores = dict(line.split("\t") for line in baseline.splitlines()[2:])
    if set(scores) != set(ACCESSIONS) or int(scores[ACCESSIONS[0]]) != 2 * len(sequences[ACCESSIONS[0]]):
        raise AssertionError("Missing scores or incorrect self-match score")
    malformed = {"ambiguous_base": ">bad\nACNT\n", "missing_header": "ACGT\n", "empty_record": ">empty\n"}
    for kind, content in malformed.items():
        path = directory / (kind + ".fasta")
        path.write_text(content, encoding="ascii")
        for mode, schedule, threads in [("serial", "dynamic", 1), ("parallel", "static", 4), ("parallel", "dynamic", 4)]:
            command = base.copy()
            command[-1] = str(path)
            run(f"reject_{kind}_{mode}_{schedule}", command + ["--mode", mode, "--schedule", schedule, "--threads", str(threads)], False)
    result = {"passed": len(checks), "failed": 0, "scores": {k: int(v) for k, v in scores.items()},
              "all_score_outputs_identical": True, "successful_configurations": 7,
              "expected_malformed_rejections": 9, "checks": checks}
    save_json(directory / "verification.json", result)
    print(json.dumps({k: v for k, v in result.items() if k != "checks"}, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["download", "verify"])
    parser.add_argument("directory", type=Path)
    parser.add_argument("--cli", type=Path, default=Path("build-release/sw_cli.exe"))
    args = parser.parse_args()
    if args.action == "download":
        download(args.directory)
    else:
        verify(args.directory, args.cli)


if __name__ == "__main__":
    main()
