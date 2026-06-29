#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""校验插件 RuntimeContext 中的 XOR 字符串和 PHP 验证种子是否一致。"""

from __future__ import annotations

import hashlib
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RUNTIME_CPP = ROOT / "src/data/runtime_context.cpp"
PHP_GATE = ROOT / "scripts/update_gate_response.php"

EXPECTED = {
    "startupDisclaimer": {
        "purpose": "disclaimer",
        "expected": "本工具完全免费，请勿付费购买\nGitHub：https://github.com/h3110w0r1d-y/rocom-helper",
        "encoding": "utf-8",
    },
}


def runtime_key(seed: bytes, purpose: str) -> bytes:
    return hashlib.sha256(seed + purpose.encode("ascii")).digest()


def parse_php_seed() -> bytes:
    content = PHP_GATE.read_text(encoding="utf-8")
    match = re.search(r"\$PAYLOAD_JSON = '([^']+)';", content)
    if match is None:
        raise RuntimeError("PAYLOAD_JSON not found")
    payload = json.loads(match.group(1))
    return bytes.fromhex(payload["s"])


def parse_runtime_array(function_name: str) -> list[int]:
    content = RUNTIME_CPP.read_text(encoding="utf-8")
    pattern = (
        rf"QString RuntimeContext::{function_name}\(\) const\s*\{{.*?"
        r"static constexpr unsigned char data\[\] = \{(.*?)\};"
    )
    match = re.search(pattern, content, flags=re.S)
    if match is None:
        raise RuntimeError(f"{function_name} data array not found")
    return [int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", match.group(1))]


def decode(encoded: list[int], key: bytes, encoding: str) -> str:
    raw = bytes(byte ^ key[index % len(key)] for index, byte in enumerate(encoded))
    return raw.decode(encoding)


def main() -> int:
    seed = parse_php_seed()
    failed = 0
    print(f"[OK] seed ({len(seed)} bytes): {seed.hex()}")

    for function_name, config in EXPECTED.items():
        key = runtime_key(seed, config["purpose"])
        decoded = decode(parse_runtime_array(function_name), key, config["encoding"])
        ok = decoded == config["expected"]
        print(f"[{'OK' if ok else 'FAIL'}] {function_name}: {decoded!r}")
        if not ok:
            failed += 1

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
