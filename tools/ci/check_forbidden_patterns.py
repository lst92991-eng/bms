#!/usr/bin/env python3
"""Enforce high-value BMS V2 architectural restrictions in self-authored C code."""

from __future__ import annotations

import re
import sys
from dataclasses import dataclass
from pathlib import Path

SOURCE_ROOT = Path("firmware")
SOURCE_SUFFIXES = {".c", ".h"}


@dataclass(frozen=True)
class Rule:
    name: str
    pattern: re.Pattern[str]
    message: str
    excluded_prefixes: tuple[str, ...] = ()


RULES = (
    Rule(
        name="dynamic-allocation",
        pattern=re.compile(
            r"\b(?:malloc|calloc|realloc|free|pvPortMalloc|vPortFree|"
            r"xTaskCreate|xQueueCreate|xSemaphoreCreateMutex|xEventGroupCreate)\s*\("
        ),
        message="Runtime dynamic allocation is forbidden in BMS V2 firmware.",
    ),
    Rule(
        name="blocking-delay",
        pattern=re.compile(r"\bHAL_Delay\s*\("),
        message="HAL_Delay is forbidden in BMS V2 runtime code.",
    ),
    Rule(
        name="blocking-format-log",
        pattern=re.compile(r"\b(?:printf|sprintf|vsprintf|fprintf)\s*\("),
        message="Formatted blocking stdio is forbidden in BMS V2 firmware.",
    ),
    Rule(
        name="hal-outside-platform",
        pattern=re.compile(r"\bHAL_[A-Za-z0-9_]+\b"),
        message="HAL APIs may only be referenced by firmware/platform adapters.",
        excluded_prefixes=("firmware/platform/",),
    ),
    Rule(
        name="hal-handle-outside-platform",
        pattern=re.compile(r"\b(?:hi2c|huart|hfdcan|htim|hrtc)[A-Za-z0-9_]*\b"),
        message="Generated HAL handles may only be referenced by firmware/platform adapters.",
        excluded_prefixes=("firmware/platform/",),
    ),
    Rule(
        name="generated-main-header-outside-platform",
        pattern=re.compile(r"#\s*include\s*[<\"]main\.h[>\"]"),
        message="main.h may only be included by firmware/platform adapters.",
        excluded_prefixes=("firmware/platform/",),
    ),
    Rule(
        name="rtos-in-domain",
        pattern=re.compile(
            r"#\s*include\s*[<\"](?:FreeRTOS|task|queue|semphr|event_groups|timers)\.h[>\"]"
        ),
        message="Domain logic must remain independent of FreeRTOS.",
        excluded_prefixes=("firmware/app/", "firmware/services/", "firmware/platform/"),
    ),
    Rule(
        name="stm32-header-outside-platform",
        pattern=re.compile(r"#\s*include\s*[<\"]stm32[^>\"]*[>\"]"),
        message="STM32 headers may only be included by firmware/platform adapters.",
        excluded_prefixes=("firmware/platform/",),
    ),
)

TODO_PATTERN = re.compile(r"\b(?:TODO|FIXME|HACK)\b")
TRACKED_TODO_PATTERN = re.compile(r"\bBMS-(?:SR|REQ|ISSUE)-\d+\b")


def source_files() -> list[Path]:
    if not SOURCE_ROOT.exists():
        return []

    return sorted(
        path
        for path in SOURCE_ROOT.rglob("*")
        if path.is_file() and path.suffix in SOURCE_SUFFIXES
    )


def is_excluded(relative_path: str, rule: Rule) -> bool:
    return relative_path.startswith(rule.excluded_prefixes)


def main() -> int:
    violations: list[str] = []

    for path in source_files():
        relative_path = path.as_posix()
        text = path.read_text(encoding="utf-8")

        for line_number, line in enumerate(text.splitlines(), start=1):
            for rule in RULES:
                if is_excluded(relative_path, rule):
                    continue
                if rule.pattern.search(line):
                    violations.append(
                        f"{relative_path}:{line_number}: {rule.name}: {rule.message}"
                    )

            if TODO_PATTERN.search(line) and not TRACKED_TODO_PATTERN.search(line):
                violations.append(
                    f"{relative_path}:{line_number}: untracked-todo: "
                    "TODO/FIXME/HACK requires a BMS-SR/BMS-REQ/BMS-ISSUE identifier."
                )

    if violations:
        print("BMS V2 forbidden-pattern check failed:", file=sys.stderr)
        for violation in violations:
            print(f"  - {violation}", file=sys.stderr)
        return 1

    print("BMS V2 forbidden-pattern check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
