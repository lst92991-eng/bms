#!/usr/bin/env python3
"""Enforce a deterministic flash/RAM budget using GNU size output."""

from __future__ import annotations

import argparse
import subprocess


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--size-tool", required=True)
    parser.add_argument("--elf", required=True)
    parser.add_argument("--max-flash", type=int, required=True)
    parser.add_argument("--max-ram", type=int, required=True)
    args = parser.parse_args()

    output = subprocess.check_output(
        [args.size_tool, args.elf], text=True, encoding="utf-8"
    )
    rows = [line.split() for line in output.splitlines() if line.strip()]
    data_row = next((row for row in rows if len(row) >= 6 and row[0].isdigit()), None)
    if data_row is None:
        raise RuntimeError(f"cannot parse size output:\n{output}")

    text_bytes, data_bytes, bss_bytes = map(int, data_row[:3])
    flash_bytes = text_bytes + data_bytes
    ram_bytes = data_bytes + bss_bytes
    print(
        f"size budget: flash={flash_bytes}/{args.max_flash}, "
        f"ram={ram_bytes}/{args.max_ram}"
    )
    if flash_bytes > args.max_flash or ram_bytes > args.max_ram:
        raise SystemExit("firmware size budget exceeded")


if __name__ == "__main__":
    main()
