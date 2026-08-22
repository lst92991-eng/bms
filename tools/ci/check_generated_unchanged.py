#!/usr/bin/env python3
"""Fail when a change touches CubeMX/HAL/CMSIS/vendor-owned source files."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import PurePosixPath

IMMUTABLE_PREFIXES = (
    "bms24v_platform/Core/",
    "bms24v_platform/Drivers/",
    "bms24v_platform/MDK-ARM/FreeRTOS/source/",
    "bms24v_platform/MDK-ARM/FreeRTOS/portable/",
)

IMMUTABLE_FILES = {
    "bms24v_platform/.mxproject",
    "bms24v_platform/bms24v_platform.ioc",
    "bms24v_platform/MDK-ARM/startup_stm32g0b1xx.s",
    "bms24v_platform/gcc/startup_stm32g0b1xx_gcc.s",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--base",
        default="origin/main",
        help="Git base ref used to calculate the merge-base diff.",
    )
    parser.add_argument(
        "--head",
        default="HEAD",
        help="Git head ref used to calculate the merge-base diff.",
    )
    return parser.parse_args()


def changed_paths(base: str, head: str) -> list[str]:
    command = ["git", "diff", "--name-only", f"{base}...{head}"]
    result = subprocess.run(
        command,
        check=False,
        capture_output=True,
        text=True,
    )

    if result.returncode != 0:
        print(result.stderr, file=sys.stderr)
        raise RuntimeError(f"git diff failed: {' '.join(command)}")

    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


def is_immutable(path: str) -> bool:
    normalized = PurePosixPath(path).as_posix()
    return normalized in IMMUTABLE_FILES or normalized.startswith(IMMUTABLE_PREFIXES)


def main() -> int:
    args = parse_args()

    try:
        changed = changed_paths(args.base, args.head)
    except RuntimeError as error:
        print(f"generated-source guard error: {error}", file=sys.stderr)
        return 2

    violations = sorted(path for path in changed if is_immutable(path))
    if violations:
        print("Generated/vendor-owned files were modified:", file=sys.stderr)
        for path in violations:
            print(f"  - {path}", file=sys.stderr)
        print(
            "Move application changes into firmware/ adapters or regenerate in an approved "
            "hardware-configuration change.",
            file=sys.stderr,
        )
        return 1

    print(f"Generated-source guard passed ({len(changed)} changed path(s) checked).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
