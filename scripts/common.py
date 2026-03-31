#!/usr/bin/env python3
"""Common helpers for Roscraft CLI utility scripts."""

from __future__ import annotations

import os
import shlex
import subprocess
import sys
from pathlib import Path
from typing import Iterable, Sequence

_CPP_EXTENSIONS = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".c++",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".h++",
    ".ipp",
    ".inl",
    ".tpp",
}

_IGNORED_DIRS = {
    ".git",
    ".gradle",
    ".idea",
    ".vscode",
    ".vs",
    ".zed",
    ".cpm_cache",
    "__pycache__",
    "_deps",
    "bin",
    "build",
    "out",
}


class _AnsiColor:
    RESET = "\033[0m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    RED = "\033[31m"
    CYAN = "\033[36m"


def _use_color() -> bool:
    return sys.stdout.isatty() and os.getenv("NO_COLOR") is None


def _colorize(text: str, color: str) -> str:
    if not _use_color():
        return text
    return f"{color}{text}{_AnsiColor.RESET}"


def _log(level: str, message: str, color: str = "") -> None:
    label = f"[{level}]"
    if color:
        label = _colorize(label, color)
    print(f"{label} {message}")


def info(message: str) -> None:
    _log("INFO", message, _AnsiColor.CYAN)


def success(message: str) -> None:
    _log("OK", message, _AnsiColor.GREEN)


def warn(message: str) -> None:
    _log("WARN", message, _AnsiColor.YELLOW)


def error(message: str) -> None:
    _log("ERROR", message, _AnsiColor.RED)


def find_upward_file(filename: str, start_dir: Path) -> Path | None:
    current = start_dir.resolve()
    for directory in [current, *current.parents]:
        candidate = directory / filename
        if candidate.is_file():
            return candidate
    return None


def _is_cpp_file(path: Path) -> bool:
    return path.suffix.lower() in _CPP_EXTENSIONS


def _walk_cpp_files(root_dir: Path) -> list[Path]:
    files: list[Path] = []
    for current, dir_names, file_names in os.walk(root_dir):
        dir_names[:] = [
            name
            for name in dir_names
            if name not in _IGNORED_DIRS and not name.startswith(".")
        ]
        current_path = Path(current)
        for file_name in file_names:
            candidate = current_path / file_name
            if _is_cpp_file(candidate):
                files.append(candidate.resolve())
    files.sort()
    return files


def collect_cpp_files(paths: Sequence[str], root_dir: Path) -> list[Path]:
    if not paths:
        return _walk_cpp_files(root_dir)

    files: list[Path] = []
    for raw_path in paths:
        path = Path(raw_path)
        if not path.is_absolute():
            path = (root_dir / path).resolve()

        if path.is_file():
            if _is_cpp_file(path):
                files.append(path)
            else:
                warn(f"Skipping non-C++ file: {path}")
            continue

        if path.is_dir():
            files.extend(_walk_cpp_files(path))
            continue

        warn(f"Skipping missing path: {path}")

    deduplicated = sorted(set(files))
    return deduplicated


def discover_named_dirs(root_dir: Path, directory_name: str) -> list[Path]:
    found: list[Path] = []
    for current, dir_names, _ in os.walk(root_dir):
        current_path = Path(current)
        if current_path.name == directory_name:
            found.append(current_path.resolve())
            dir_names.clear()
            continue

        dir_names[:] = [
            name
            for name in dir_names
            if name not in {".git", ".gradle", "__pycache__"}
            and not name.startswith(".")
        ]
    found.sort()
    return found


def discover_compile_database_dirs(build_dirs: Sequence[Path]) -> list[Path]:
    compile_db_dirs: set[Path] = set()
    for build_dir in build_dirs:
        direct_db = build_dir / "compile_commands.json"
        if direct_db.is_file():
            compile_db_dirs.add(build_dir.resolve())

        for compile_db in build_dir.rglob("compile_commands.json"):
            compile_db_dirs.add(compile_db.parent.resolve())

    return sorted(compile_db_dirs)


def split_chunks(items: Sequence[Path], chunk_size: int) -> Iterable[list[Path]]:
    for index in range(0, len(items), chunk_size):
        yield list(items[index : index + chunk_size])


def quote_command(command: Sequence[str]) -> str:
    return " ".join(shlex.quote(part) for part in command)


def run_command(
    command: Sequence[str],
    *,
    cwd: Path,
    capture_output: bool = True,
) -> subprocess.CompletedProcess[str]:
    info(f"Running: {quote_command(command)}")
    return subprocess.run(
        list(command),
        cwd=str(cwd),
        text=True,
        capture_output=capture_output,
        check=False,
    )


def print_completed_output(result: subprocess.CompletedProcess[str]) -> None:
    if result.stdout:
        print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)


def is_relative_to(path: Path, base: Path) -> bool:
    try:
        path.relative_to(base)
        return True
    except ValueError:
        return False


def infer_source_root_from_build_dir(compile_db_dir: Path) -> Path | None:
    current = compile_db_dir.resolve()
    for directory in [current, *current.parents]:
        if directory.name == "build":
            return directory.parent
    return None
