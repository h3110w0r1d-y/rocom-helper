#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from __future__ import annotations

import math
import hashlib
import secrets


BITS = 4096
PUBLIC_EXPONENT = 65537
EXPECTED_BYTES_SIZE = 32
EXPECTED_BYTES = secrets.token_bytes(EXPECTED_BYTES_SIZE)
CPP_MODULUS_XOR_KEY = 0xA7
RUNTIME_STRINGS = {
    "web-resource": [":/web", "/index.html"],
    "http": ["http://127.0.0.1:4939/"],
    "traffic": [":/rwtd"],
}


def is_probable_prime(value: int) -> bool:
    if value < 2:
        return False
    small_primes = [3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41]
    for prime in small_primes:
        if value % prime == 0:
            return value == prime

    d = value - 1
    s = 0
    while d % 2 == 0:
        s += 1
        d //= 2

    for witness in [2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37]:
        if witness >= value - 2:
            continue
        x = pow(witness, d, value)
        if x == 1 or x == value - 1:
            continue
        for _ in range(s - 1):
            x = pow(x, 2, value)
            if x == value - 1:
                break
        else:
            return False
    return True


def generate_prime(bits: int, exponent: int) -> int:
    while True:
        value = secrets.randbits(bits) | (1 << (bits - 1)) | 1
        if math.gcd(exponent, value - 1) != 1:
            continue
        if is_probable_prime(value):
            return value


def hex_wrap(value: int) -> str:
    return format(value, "x")


def print_cpp_modulus_array(n: int) -> None:
    modulus_size = (n.bit_length() + 7) // 8
    encoded = [byte ^ CPP_MODULUS_XOR_KEY for byte in n.to_bytes(modulus_size, "big")]

    print(f"static constexpr unsigned char key = 0x{CPP_MODULUS_XOR_KEY:02x};")
    print("static constexpr unsigned char data[] = {")
    for index in range(0, len(encoded), 12):
        chunk = encoded[index:index + 12]
        print("    " + ", ".join(f"0x{byte:02X}" for byte in chunk) + ",")
    print("};")


def print_cpp_expected_bytes(data: bytes) -> None:
    print("constexpr unsigned char UpdateCheckExpectedBytes[] = {")
    for index in range(0, len(data), 12):
        chunk = data[index:index + 12]
        print("    " + ", ".join(f"0x{byte:02X}" for byte in chunk) + ",")
    print("};")


def runtime_key(seed: bytes, purpose: str) -> bytes:
    return hashlib.sha256(seed + purpose.encode("ascii")).digest()


def print_runtime_string_arrays(seed: bytes) -> None:
    for purpose, values in RUNTIME_STRINGS.items():
        key = runtime_key(seed, purpose)
        print(f"{purpose}:")
        for value in values:
            raw = value.encode("ascii")
            encoded = [byte ^ key[index % len(key)] for index, byte in enumerate(raw)]
            print(f"  {value}")
            print("  " + ", ".join(f"0x{byte:02X}" for byte in encoded))


def main() -> int:
    p = generate_prime(BITS // 2, PUBLIC_EXPONENT)
    while True:
        q = generate_prime(BITS // 2, PUBLIC_EXPONENT)
        if q != p:
            break

    n = p * q
    phi = (p - 1) * (q - 1)
    d = pow(PUBLIC_EXPONENT, -1, phi)
    modulus_size = (n.bit_length() + 7) // 8
    max_payload_size = modulus_size - 11

    print("Generated RSA parameters")
    print(f"bits={n.bit_length()}")
    print(f"e={PUBLIC_EXPONENT}")
    print(f"max_payload_size={max_payload_size}")
    print()
    print("C++ public key:")
    print(f'constexpr uint32_t UpdateCheckRsaExponent = {PUBLIC_EXPONENT};')
    print_cpp_expected_bytes(EXPECTED_BYTES)
    print()
    print("C++ decodedUpdateCheckModulus() body constants:")
    print_cpp_modulus_array(n)
    print()
    print("C++ RuntimeContext encoded strings:")
    print_runtime_string_arrays(EXPECTED_BYTES)
    print()
    print("PHP private key:")
    print(f"$RSA_E = '{PUBLIC_EXPONENT}';")
    print(f"$RSA_P_HEX = '{hex_wrap(p)}';")
    print(f"$RSA_Q_HEX = '{hex_wrap(q)}';")
    print(f"$EXPECTED_BYTES_HEX = '{EXPECTED_BYTES.hex()}';")
    print()
    print("Optional derived values:")
    print(f"n={hex_wrap(n)}")
    print(f"d={hex_wrap(d)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
