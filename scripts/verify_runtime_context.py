#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Verify RuntimeContext XOR-encoded strings against seed/keys in current C++ code."""

from __future__ import annotations

import hashlib
import sys

# src/main.cpp -> UpdateCheckExpectedBytes (32-byte seed for makeRuntimeContext)
SEED = bytes([
    0x60, 0x66, 0x25, 0x55, 0x28, 0xD1, 0x45, 0x77,
    0x94, 0x7F, 0x5F, 0xDB, 0x75, 0x9F, 0x93, 0x15,
    0x91, 0x64, 0x9B, 0x34, 0xF5, 0xEC, 0xAE, 0xA5,
    0x61, 0xAF, 0x45, 0x2B, 0x60, 0x58, 0x07, 0x84,
])

# src/data/runtime_context.cpp -> encoded byte arrays
ENCODED = {
    "webResourceRoot": {
        "purpose": "web-resource",
        "data": [0x6B, 0xFC, 0x60, 0xF6, 0xDA],
        "expected": ":/web",
    },
    "webIndexPath": {
        "purpose": "web-resource",
        "data": [
            0x7E, 0xBA, 0x79, 0xF7, 0xDD, 0x40, 0x35, 0x86, 0xE2, 0xF3, 0x92,
        ],
        "expected": "/index.html",
    },
    "trafficSchemaRoot": {
        "purpose": "traffic",
        "data": [0xA6, 0x0F, 0xB4, 0x84, 0x4F, 0x37],
        "expected": ":/rwtd",
        "encoding": "ascii",
    },
    "startupDisclaimer": {
        "purpose": "disclaimer",
        "data": [
            0x8B, 0x51, 0x38, 0x6D, 0x83, 0x0B, 0x77, 0xF2, 0x85, 0x87, 0x46, 0x49,
            0xA6, 0x8D, 0x44, 0xC0, 0xD4, 0x5F, 0x20, 0xB8, 0xA2, 0x94, 0x8C, 0x0D,
            0xA6, 0xCD, 0x6B, 0x0F, 0x35, 0x8A, 0x72, 0xC0, 0xF5, 0x25, 0x20, 0x31,
            0xDC, 0x1A, 0x3F, 0x93, 0x8B, 0xD2, 0xE2, 0x82, 0x2A, 0x7C, 0xA4, 0x50,
            0x33, 0x3D, 0x74, 0x96, 0x73, 0x0F, 0x44, 0xF1, 0x3D, 0x58, 0xF3, 0xC5,
            0xD9, 0x5C, 0xE2, 0x13, 0x18, 0xAF, 0xBA, 0xEB, 0x5B, 0xC3, 0xBD, 0x1F,
            0x01, 0x53, 0xD9, 0xF5, 0x34, 0x38, 0x9E, 0x14, 0x35, 0xFF, 0xB1, 0x23,
            0x69, 0x14, 0x53, 0xEE, 0x23, 0x4F, 0xB4, 0x8F, 0xD2, 0x45, 0xF3, 0x09,
        ],
        "expected": "本工具完全免费，请勿付费购买\nGitHub：https://github.com/h3110w0r1d-y/rocom-helper",
        "encoding": "utf-8",
    },
}

EXPECTED_RUNTIME_STRINGS = {
    "web-resource": [":/web", "/index.html"],
    "traffic": [":/rwtd"],
    "disclaimer": [
        "本工具完全免费，请勿付费购买\nGitHub：https://github.com/h3110w0r1d-y/rocom-helper",
    ],
}


def runtime_key(seed: bytes, purpose: str) -> bytes:
    return hashlib.sha256(seed + purpose.encode("ascii")).digest()


def decode_runtime_text(encoded: list[int], key: bytes, encoding: str = "ascii") -> str:
    decoded = bytes(
        byte ^ key[index % len(key)]
        for index, byte in enumerate(encoded)
    )
    return decoded.decode(encoding)


def encode_runtime_text(plaintext: str, key: bytes, encoding: str = "ascii") -> list[int]:
    raw = plaintext.encode(encoding)
    return [byte ^ key[index % len(key)] for index, byte in enumerate(raw)]


def main() -> int:
    if len(SEED) < 16:
        print("FAIL: seed must be at least 16 bytes")
        return 1

    print(f"seed ({len(SEED)} bytes): {SEED.hex()}")
    print()

    failed = 0
    for name, entry in ENCODED.items():
        purpose = entry["purpose"]
        encoding = entry.get("encoding", "ascii")
        key = runtime_key(SEED, purpose)
        decoded = decode_runtime_text(entry["data"], key, encoding)
        expected = entry["expected"]
        ok = decoded == expected
        status = "OK" if ok else "FAIL"
        print(f"[{status}] {name}")
        print(f"  purpose : {purpose}")
        print(f"  encoding: {encoding}")
        print(f"  key     : {key.hex()}")
        print(f"  encoded : {', '.join(f'0x{b:02X}' for b in entry['data'])}")
        print(f"  decoded : {decoded!r}")
        print(f"  expected: {expected!r}")
        if not ok:
            failed += 1
            reencoded = encode_runtime_text(expected, key, encoding)
            print(f"  fix enc : {', '.join(f'0x{b:02X}' for b in reencoded)}")
        print()

    print("Cross-check with generate_update_rsa_keypair.py RUNTIME_STRINGS:")
    for purpose, values in EXPECTED_RUNTIME_STRINGS.items():
        key = runtime_key(SEED, purpose)
        for value in values:
            encoding = "utf-8" if purpose == "disclaimer" else "ascii"
            encoded = encode_runtime_text(value, key, encoding)
            matched = None
            for name, entry in ENCODED.items():
                if entry["purpose"] == purpose and entry["expected"] == value:
                    matched = name
                    break
            enc_hex = ", ".join(f"0x{b:02X}" for b in encoded)
            if matched is None:
                print(f"  [WARN] no C++ array mapped for {purpose!r} -> {value!r}: {enc_hex}")
                continue
            cpp_data = ENCODED[matched]["data"]
            ok = encoded == cpp_data
            status = "OK" if ok else "FAIL"
            print(f"  [{status}] {matched}: {value!r}")
            if not ok:
                failed += 1
                print(f"         C++ has : {', '.join(f'0x{b:02X}' for b in cpp_data)}")
                print(f"         should be: {enc_hex}")

    print()
    if failed:
        print(f"Result: {failed} mismatch(es)")
        return 1

    print("Result: all RuntimeContext encoded strings match seed/keys")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
