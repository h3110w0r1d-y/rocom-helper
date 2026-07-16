#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from __future__ import annotations

import argparse
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CMAKE_PATH = ROOT / "CMakeLists.txt"
VERSION_PATTERN = re.compile(
    r"^v?(?P<text>(?P<numeric>\d+(?:\.\d+){1,3})(?:[-+][A-Za-z0-9][A-Za-z0-9._-]*)?)$"
)


def parse_version(version: str) -> tuple[str, str]:
    match = VERSION_PATTERN.fullmatch(version.strip())
    if match is None:
        raise RuntimeError("版本号格式应类似 3.5.1 或 v3.5.1-beta1")
    return match.group("text"), match.group("numeric")


def replace_one(content: str, pattern: str, replacement: str, label: str) -> str:
    updated, count = re.subn(pattern, replacement, content, count=1)
    if count != 1:
        raise RuntimeError(f"未找到或无法更新 {label}")
    return updated


def update_version(version: str) -> None:
    text, numeric = parse_version(version)
    content = CMAKE_PATH.read_text(encoding="utf-8")
    content = replace_one(
        content,
        r"project\(roco_helper_plugin VERSION [^ ]+ LANGUAGES CXX\)",
        f"project(roco_helper_plugin VERSION {numeric} LANGUAGES CXX)",
        "CMake 项目版本",
    )
    content = replace_one(
        content,
        r'set\(ROCO_APP_VERSION "[^"]+"\)',
        f'set(ROCO_APP_VERSION "{text}")',
        "应用版本",
    )

    old_content = CMAKE_PATH.read_text(encoding="utf-8")
    if content == old_content:
        print(f"版本号已经是 {text}")
        return

    CMAKE_PATH.write_text(content, encoding="utf-8", newline="\n")
    print(f"版本号已更新为 {text}")


def main() -> int:
    parser = argparse.ArgumentParser(description="更新插件版本号")
    parser.add_argument("--version", required=True, help="例如 3.5.1 或 v3.5.1-beta1")
    args = parser.parse_args()
    update_version(args.version)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
