#!/usr/bin/env python3
import secrets

SIZE = 32

data = secrets.token_bytes(SIZE)

cpp = "".join(f"\\x{b:02x}" for b in data)
php = "".join(f"\\x{b:02x}" for b in data)

print("raw hex:")
print(data.hex())

print("\nC++:")
print(f'constexpr auto UpdateCheckExpectedBytes = "{cpp}";')

print("\nPHP:")
print(f"$EXPECTED_BYTES = \"{php}\";")