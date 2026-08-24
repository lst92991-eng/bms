#!/usr/bin/env python3
"""Reject heap and stdio symbols from a release-candidate firmware image."""

from __future__ import annotations

import argparse
import subprocess


FORBIDDEN_SYMBOLS = {
    "_calloc_r",
    "_free_r",
    "_malloc_r",
    "_realloc_r",
    "_sbrk",
    "_sbrk_r",
    "_write",
    "calloc",
    "fputc",
    "fprintf",
    "free",
    "malloc",
    "printf",
    "pvPortMalloc",
    "realloc",
    "snprintf",
    "vPortFree",
    "vfprintf",
    "vprintf",
    "vsnprintf",
}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--nm", required=True)
    parser.add_argument("--elf", required=True)
    args = parser.parse_args()

    output = subprocess.check_output(
        [args.nm, "-C", args.elf], text=True, encoding="utf-8", errors="replace"
    )
    symbols = {line.split()[-1] for line in output.splitlines() if line.split()}
    forbidden = sorted(symbols & FORBIDDEN_SYMBOLS)
    if forbidden:
        raise SystemExit("forbidden heap/stdio symbols: " + ", ".join(forbidden))
    for required in ("Com_FormatV", "Int_Log_Printf"):
        if required not in symbols:
            raise SystemExit(f"required bounded log symbol missing: {required}")
    print("symbol gate: no heap/stdio symbols; bounded log formatter present")


if __name__ == "__main__":
    main()
