#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from __future__ import annotations


# 要混淆的 URL。
URL = "https://h3110w0r1d.com/roco-helper/v1.php"

# C++ 运行时会用同一个 key 异或还原。
XOR_KEY = 0x5A

# 每行输出多少个字节。
BYTES_PER_LINE = 12


def main() -> int:
    encoded = [byte ^ XOR_KEY for byte in URL.encode("ascii")]

    print(f"static constexpr unsigned char key = 0x{XOR_KEY:02x};")
    print("static constexpr unsigned char data[] = {")
    for index in range(0, len(encoded), BYTES_PER_LINE):
        chunk = encoded[index:index + BYTES_PER_LINE]
        print("    " + ", ".join(f"0x{byte:02X}" for byte in chunk) + ",")
    print("};")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
