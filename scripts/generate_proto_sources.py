#!/usr/bin/env python3
"""Generate C++ protobuf sources from a FileDescriptorSet.

Example:
  python scripts/generate_proto_sources.py ^
    --protoc C:/vcpkg/installed/x64-mingw-dynamic/tools/protobuf/protoc.exe
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def normalize_path(path: Path) -> str:
    return path.as_posix()


def names_from_python_protobuf(descriptor_path: Path) -> list[str] | None:
    try:
        from google.protobuf import descriptor_pb2
    except ImportError:
        return None

    file_set = descriptor_pb2.FileDescriptorSet()
    file_set.ParseFromString(descriptor_path.read_bytes())
    return [file.name for file in file_set.file if file.name]


def names_from_protoc(protoc: str, descriptor_path: Path) -> list[str]:
    command = [
        protoc,
        "--decode=google.protobuf.FileDescriptorSet",
        "google/protobuf/descriptor.proto",
    ]
    result = subprocess.run(
        command,
        input=descriptor_path.read_bytes(),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(result.stderr.decode(errors="replace").strip())

    names: list[str] = []
    in_file_block = False
    file_depth = 0
    for raw_line in result.stdout.decode(errors="replace").splitlines():
        stripped = raw_line.strip()
        if stripped == "file {":
            in_file_block = True
            file_depth = 1
            continue
        if not in_file_block:
            continue
        if stripped.endswith("{"):
            file_depth += 1
            continue
        if stripped == "}":
            file_depth -= 1
            if file_depth == 0:
                in_file_block = False
            continue
        if file_depth == 1:
            match = re.fullmatch(r'name: "(.+)"', stripped)
            if match:
                names.append(match.group(1))

    return names


def descriptor_names(protoc: str, descriptor_path: Path) -> list[str]:
    names = names_from_python_protobuf(descriptor_path)
    if names is None:
        names = names_from_protoc(protoc, descriptor_path)
    return [
        name
        for name in names
        if name.endswith(".proto") and not name.startswith("google/protobuf/")
    ]


def run_protoc(protoc: str, descriptor_path: Path, output_dir: Path, names: list[str], dry_run: bool) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    command = [
        protoc,
        f"--descriptor_set_in={normalize_path(descriptor_path)}",
        f"--cpp_out={normalize_path(output_dir)}",
        *names,
    ]
    print("Generating", len(names), "proto files")
    print(" ".join(f'"{part}"' if " " in part else part for part in command))
    if dry_run:
        return
    subprocess.run(command, check=True)


def main() -> int:
    root = repo_root()
    parser = argparse.ArgumentParser()
    parser.add_argument("--descriptor", default=root / "resources/rwtd/all.pb", type=Path)
    parser.add_argument("--out", default=root / "src/rwtd/proto", type=Path)
    parser.add_argument("--protoc", default=os.environ.get("PROTOC", "protoc"))
    parser.add_argument("--list", action="store_true", help="Only list proto names in the descriptor set.")
    parser.add_argument("--dry-run", action="store_true", help="Print the protoc command without running it.")
    args = parser.parse_args()

    descriptor_path = args.descriptor.resolve()
    output_dir = args.out.resolve()
    names = descriptor_names(args.protoc, descriptor_path)
    if not names:
        print(f"No .proto descriptors found in {descriptor_path}", file=sys.stderr)
        return 1

    if args.list:
        print("\n".join(names))
        return 0

    run_protoc(args.protoc, descriptor_path, output_dir, names, args.dry_run)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
