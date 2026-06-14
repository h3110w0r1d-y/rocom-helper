#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from __future__ import annotations

import re
import sys
from pathlib import Path

# 要混淆的 URL。
URL = "https://h3110w0r1d.com/roco-helper/v2.0.php"

# C++ 运行时会用同一个 key 异或还原。
XOR_KEY = 0x5A

# 每行输出多少个字节。
BYTES_PER_LINE = 12

ROOT = Path(__file__).resolve().parent.parent
MAIN_CPP = ROOT / "src" / "main.cpp"
CMAKE_LISTS = ROOT / "CMakeLists.txt"

VERSION_FROM_URL_PATTERN = re.compile(r"/v(\d+\.\d+)\.php")

FUNC_PATTERN = re.compile(
    r"(QString decodedUpdateCheckUrl\(\)\s*\{\s*)"
    r"(    static constexpr unsigned char key = 0x[0-9a-fA-F]+;\s*"
    r"    static constexpr unsigned char data\[\] = \{.*?\};\s*)"
    r"(    QByteArray url;)",
    re.DOTALL,
)

CMAKE_PROJECT_PATTERN = re.compile(
    r"(project\(roco_helper VERSION )\d+\.\d+( LANGUAGES CXX\))",
)


def version_from_url(url: str) -> str:
    match = VERSION_FROM_URL_PATTERN.search(url)
    if match is None:
        raise RuntimeError(
            f"cannot parse version from URL (expected /vX.Y.php): {url}"
        )
    return match.group(1)


def format_key_data_block() -> str:
    encoded = [byte ^ XOR_KEY for byte in URL.encode("ascii")]

    lines = [
        f"    static constexpr unsigned char key = 0x{XOR_KEY:02x};",
        "    static constexpr unsigned char data[] = {",
    ]
    for index in range(0, len(encoded), BYTES_PER_LINE):
        chunk = encoded[index:index + BYTES_PER_LINE]
        lines.append("        " + ", ".join(f"0x{byte:02X}" for byte in chunk) + ",")
    lines.append("    };")
    return "\n".join(lines)


def patch_main_cpp(content: str, block: str) -> str:
    match = FUNC_PATTERN.search(content)
    if match is None:
        raise RuntimeError("decodedUpdateCheckUrl() block not found in main.cpp")

    return content[: match.start(2)] + block + "\n\n" + content[match.start(3) :]


def patch_cmake_lists(content: str, version: str) -> str:
    match = CMAKE_PROJECT_PATTERN.search(content)
    if match is None:
        raise RuntimeError(
            "project(roco_helper VERSION ... LANGUAGES CXX) not found in CMakeLists.txt"
        )

    new_line = f"{match.group(1)}{version}{match.group(2)}"
    return content[: match.start()] + new_line + content[match.end() :]


def update_file(path: Path, content: str, updated: str) -> bool:
    if updated == content:
        print(f"no changes: {path}")
        return False

    path.write_text(updated, encoding="utf-8", newline="\n")
    print(f"updated {path}")
    return True


def main() -> int:
    version = version_from_url(URL)

    main_cpp = MAIN_CPP
    if not main_cpp.is_file():
        print(f"error: main.cpp not found at {main_cpp}", file=sys.stderr)
        return 1

    cmake_lists = CMAKE_LISTS
    if not cmake_lists.is_file():
        print(f"error: CMakeLists.txt not found at {cmake_lists}", file=sys.stderr)
        return 1

    block = format_key_data_block()
    main_content = main_cpp.read_text(encoding="utf-8")
    main_updated = patch_main_cpp(main_content, block)

    cmake_content = cmake_lists.read_text(encoding="utf-8")
    cmake_updated = patch_cmake_lists(cmake_content, version)

    changed = False
    changed |= update_file(main_cpp, main_content, main_updated)
    changed |= update_file(cmake_lists, cmake_content, cmake_updated)

    if changed:
        print(f"url={URL}")
        print(f"version={version}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
