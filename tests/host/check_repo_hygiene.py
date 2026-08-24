#!/usr/bin/env python3
"""Fail when generated artifacts or caches are tracked by Git."""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path, PurePosixPath


FORBIDDEN_COMPONENTS = {
    "__pycache__",
    ".cache",
    ".mypy_cache",
    ".pytest_cache",
    "cmakefiles",
    "daplink",
    "listings",
    "logs",
    "node_modules",
    "objects",
    "out",
    "tmp",
}

FORBIDDEN_PREFIXES = {
    "bms24v_platform/mdk-arm/bms24v_platform/",
    "旧项目信息/旧项目代码/ups/ups001/mdk-arm/ups001/",
}

FORBIDDEN_SUFFIXES = {
    ".7z",
    ".axf",
    ".bin",
    ".build_log.htm",
    ".crf",
    ".d",
    ".dep",
    ".elf",
    ".exe",
    ".hex",
    ".htm",
    ".lnp",
    ".log",
    ".log.lock",
    ".lst",
    ".map",
    ".o",
    ".obj",
    ".pack",
    ".pyc",
    ".pyo",
    ".uvoptx",
    ".zip",
}


def tracked_paths(root: Path) -> list[str]:
    result = subprocess.run(
        ["git", "-c", "core.quotepath=false", "ls-files", "-z"],
        cwd=root,
        check=True,
        stdout=subprocess.PIPE,
    )
    return [
        raw.decode("utf-8", errors="surrogateescape")
        for raw in result.stdout.split(b"\0")
        if raw
    ]


def is_forbidden(relative: str) -> bool:
    path = PurePosixPath(relative)
    normalized = relative.lower()
    components = {component.lower() for component in path.parts}
    filename = path.name.lower()

    if any(normalized.startswith(prefix) for prefix in FORBIDDEN_PREFIXES):
        return True
    if components & FORBIDDEN_COMPONENTS:
        return True
    if any(component == "build" or component.startswith("build-") for component in components):
        return True
    if filename in {"cmakecache.txt", "cmake_install.cmake", "compile_commands.json"}:
        return True
    if ".uvguix." in filename:
        return True
    return any(filename.endswith(suffix) for suffix in FORBIDDEN_SUFFIXES)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    root = parser.parse_args().source_root.resolve()
    forbidden = sorted(path for path in tracked_paths(root) if is_forbidden(path))

    if forbidden:
        joined = "\n".join(f"  {path}" for path in forbidden)
        raise SystemExit(f"tracked generated artifacts are forbidden:\n{joined}")

    print("repository hygiene: no tracked build/cache/log artifacts")


if __name__ == "__main__":
    main()
