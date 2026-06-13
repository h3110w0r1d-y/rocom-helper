#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from __future__ import annotations

import re
import sys
from pathlib import Path

# 要混淆的 URL。
URL = "https://h3110w0r1d.com/roco-helper/v1.7.php"

# C++ 运行时会用同一个 key 异或还原。
XOR_KEY = 0x5A

# 每行输出多少个字节。
BYTES_PER_LINE = 12

MAIN_CPP = Path(__file__).resolve().parent.parent / "src" / "main.cpp"

FUNC_PATTERN = re.compile(
    r"(QString decodedUpdateCheckUrl\(\)\s*\{\s*)"
    r"(    static constexpr unsigned char key = 0x[0-9a-fA-F]+;\s*"
    r"    static constexpr unsigned char data\[\] = \{.*?\};\s*)"
    r"(    QByteArray url;)",
    re.DOTALL,
)


def main_cpp_path() -> Path:
    return MAIN_CPP


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


def main() -> int:
    main_cpp = main_cpp_path()
    if not main_cpp.is_file():
        print(f"error: main.cpp not found at {main_cpp}", file=sys.stderr)
        return 1

    block = format_key_data_block()
    content = main_cpp.read_text(encoding="utf-8")
    updated = patch_main_cpp(content, block)

    if updated == content:
        print(f"no changes: {main_cpp}")
        return 0

    main_cpp.write_text(updated, encoding="utf-8", newline="\n")
    print(f"updated {main_cpp}")
    print(f"url={URL}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
