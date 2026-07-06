#!/usr/bin/env python3
"""Merge ATORCH load tester CSV and BMS SOC CSV by nearest timestamp."""

from __future__ import annotations

import argparse
import csv
from datetime import datetime
from pathlib import Path


def parse_time(value: str) -> datetime:
    for fmt in ("%Y-%m-%d_%H:%M:%S.%f", "%Y-%m-%d_%H:%M:%S"):
        try:
            return datetime.strptime(value, fmt)
        except ValueError:
            pass
    raise ValueError(f"bad timestamp: {value}")


def read_atorch(path: Path) -> list[dict[str, str]]:
    lines = path.read_text(encoding="utf-8-sig", errors="replace").splitlines()
    header_index = None
    for index, line in enumerate(lines):
        if line.startswith("DATE\t") or line.startswith("DATE,"):
            header_index = index
            break
    if header_index is None:
        raise ValueError("ATORCH header line starting with DATE was not found")

    sample = lines[header_index]
    dialect = csv.excel_tab if "\t" in sample else csv.excel
    rows = list(csv.DictReader(lines[header_index:], dialect=dialect))
    for row in rows:
        row["_time"] = parse_time(row["DATE"])
    return rows


def read_bms(path: Path) -> list[dict[str, str]]:
    rows = list(csv.DictReader(path.open("r", encoding="ascii", newline="")))
    for row in rows:
        row["_time"] = parse_time(row["PC_DATE"])
    return rows


def nearest_bms(load_time: datetime, bms_rows: list[dict[str, str]], start: int) -> tuple[int, dict[str, str], float]:
    best_index = start
    best_row = bms_rows[start]
    best_delta = abs((best_row["_time"] - load_time).total_seconds())

    index = start + 1
    while index < len(bms_rows):
        delta = abs((bms_rows[index]["_time"] - load_time).total_seconds())
        if delta > best_delta:
            break
        best_index = index
        best_row = bms_rows[index]
        best_delta = delta
        index += 1

    return best_index, best_row, best_delta


def merge(load_rows: list[dict[str, str]], bms_rows: list[dict[str, str]], max_delta_s: float) -> list[dict[str, str]]:
    if not bms_rows:
        raise ValueError("BMS CSV has no data rows")

    merged: list[dict[str, str]] = []
    bms_index = 0
    for load_row in load_rows:
        bms_index, bms_row, delta = nearest_bms(load_row["_time"], bms_rows, bms_index)
        out: dict[str, str] = {}
        for key, value in load_row.items():
            if key != "_time":
                out[f"LOAD_{key}"] = value
        if delta <= max_delta_s:
            for key, value in bms_row.items():
                if key != "_time":
                    out[f"BMS_{key}"] = value
            out["MERGE_DELTA_S"] = f"{delta:.3f}"
        else:
            out["MERGE_DELTA_S"] = ""
        merged.append(out)
    return merged


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--load", required=True, type=Path, help="ATORCH load tester CSV")
    parser.add_argument("--bms", required=True, type=Path, help="BMS CSV produced by log_soc_csv.ps1")
    parser.add_argument("--out", required=True, type=Path, help="merged CSV output")
    parser.add_argument("--max-delta-s", type=float, default=1.5)
    args = parser.parse_args()

    load_rows = read_atorch(args.load)
    bms_rows = read_bms(args.bms)
    merged = merge(load_rows, bms_rows, args.max_delta_s)
    if not merged:
        raise ValueError("no merged rows")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", encoding="utf-8-sig", newline="") as fp:
        writer = csv.DictWriter(fp, fieldnames=list(merged[0].keys()))
        writer.writeheader()
        writer.writerows(merged)

    print(f"load rows: {len(load_rows)}")
    print(f"bms rows: {len(bms_rows)}")
    print(f"merged: {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
