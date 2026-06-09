#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from __future__ import annotations

import locale
import os
import re
import signal
import struct
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO, Any


# USB CDC 串口名。Windows 示例："COM5"；macOS 示例："/dev/cu.usbmodemXXXX"。
SERIAL_PORT = "/dev/tty.usbmodemHIDGD1"

# 串口波特率。
BAUD_RATE = 9600

# 是否不打开串口，只在 stderr 打印解析出的 angle/pulses，适合先调试脚本。
DRY_RUN = False

# 小于该角度时发送 0。
DEADZONE = 2.0

# PlayCover 标定：固定发送 30，每 0.01 秒发送一次。
PULSE_DX = 30
PULSE_INTERVAL_SECONDS = 0.01

# 角度标定点：(发送次数, 游戏实际转动角度)。脚本会按这些点插值估算发送次数。
CALIBRATION_POINTS = [
    (0, 0.0),
    (10, 16.2),
    (20, 36.4),
    (30, 56.7),
]

# 单次建议最多发送多少次，避免异常角度导致长时间阻塞。
MAX_PULSES = 40

# 反转鼠标横向移动方向。
INVERT = False

# 串口发送协议。False：发送 signed int8 单字节；True：发送文本 "<dx>\n"。
TEXT_PROTOCOL = True

# 打印 angle -> pulses/dx 调试信息到 stderr。
VERBOSE = False

# 解析输出时使用的编码；留空表示使用系统编码，并回退 UTF-8/GB18030。
OUTPUT_ENCODING = ""

# 串口最小发送间隔，单位毫秒；0 表示不限频。
MIN_INTERVAL_MS = 500

# roco_helper 退出时是否发送 0，避免外设保留最后一次横向移动值。
SEND_ZERO_ON_EXIT = True

# 串口断开后重新连接的间隔，单位秒。
RECONNECT_INTERVAL_SECONDS = 1.0

# 是否把串口连接、断开、重连失败信息打印到 stderr。
LOG_SERIAL_STATUS = True

# 手动指定要运行的程序及参数；为空时按系统自动识别。
# Windows 默认：<repo>/cmake-build-release/dist/roco_helper.exe
# macOS 默认：<repo>/cmake-build-release/roco_helper.app/Contents/MacOS/roco_helper
COMMAND: list[str] = []


SUGGESTION_RE = re.compile(r"path方向建议:\s+旋转=([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s*°?")


@dataclass
class SteeringConfig:
    deadzone: float
    invert: bool
    text: bool
    min_interval_ms: int
    verbose: bool


class SerialManager:
    def __init__(self, port: str, baud: int) -> None:
        self.port = port
        self.baud = baud
        self._serial: Any | None = None
        self._next_open_time = 0.0
        self._last_open_error = ""
        try:
            import serial
        except ImportError as exc:
            raise RuntimeError("缺少 pyserial，请先执行：python -m pip install pyserial") from exc
        self._serial_module = serial

    def _log(self, message: str) -> None:
        if LOG_SERIAL_STATUS or VERBOSE:
            print(f"[serial] {message}", file=sys.stderr)

    def open_if_needed(self) -> bool:
        if self._serial is not None:
            return True

        now = time.monotonic()
        if now < self._next_open_time:
            return False

        try:
            self._serial = self._serial_module.Serial(
                port=self.port,
                baudrate=self.baud,
                timeout=0,
                write_timeout=1,
            )
        except Exception as exc:
            self._next_open_time = now + RECONNECT_INTERVAL_SECONDS
            message = f"{type(exc).__name__}: {exc}"
            if message != self._last_open_error:
                self._log(f"串口未连接，等待重连: {message}")
                self._last_open_error = message
            return False

        self._last_open_error = ""
        self._log(f"串口已连接: {self.port}")
        return True

    def write(self, data: bytes) -> bool:
        if not self.open_if_needed():
            return False

        try:
            self._serial.write(data)
            self._serial.flush()
        except Exception as exc:
            self._log(f"串口写入失败，等待重连: {type(exc).__name__}: {exc}")
            self.close()
            self._next_open_time = time.monotonic() + RECONNECT_INTERVAL_SECONDS
            return False

        return True

    def close(self) -> None:
        if self._serial is None:
            return

        try:
            self._serial.close()
        except Exception:
            pass
        finally:
            self._serial = None


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def default_command() -> list[str]:
    root = repo_root()
    if sys.platform == "darwin":
        return [str(root / "cmake-build-release" / "roco_helper.app" / "Contents" / "MacOS" / "roco_helper")]
    if os.name == "nt":
        return [str(root / "cmake-build-release" / "dist" / "roco_helper.exe")]
    return [str(root / "cmake-build-release" / "roco_helper")]


def active_command() -> list[str]:
    return COMMAND if COMMAND else default_command()


def validate_config(command: list[str]) -> None:
    if DEADZONE < 0:
        raise ValueError("DEADZONE 不能小于 0")
    if PULSE_DX <= 0 or PULSE_DX > 127:
        raise ValueError("PULSE_DX 必须在 1~127 之间")
    if PULSE_INTERVAL_SECONDS <= 0:
        raise ValueError("PULSE_INTERVAL_SECONDS 必须大于 0")
    if MAX_PULSES <= 0:
        raise ValueError("MAX_PULSES 必须大于 0")
    if len(CALIBRATION_POINTS) < 2:
        raise ValueError("CALIBRATION_POINTS 至少需要两个点")
    for index in range(1, len(CALIBRATION_POINTS)):
        prev_count, prev_angle = CALIBRATION_POINTS[index - 1]
        count, angle = CALIBRATION_POINTS[index]
        if count <= prev_count or angle <= prev_angle:
            raise ValueError("CALIBRATION_POINTS 必须按发送次数和角度递增")
    if not DRY_RUN and not SERIAL_PORT:
        raise ValueError("非 DRY_RUN 模式必须设置 SERIAL_PORT")
    if not command:
        raise ValueError("COMMAND 为空，且无法自动识别运行目标")
    if not Path(command[0]).exists():
        raise FileNotFoundError(f"运行目标不存在: {command[0]}")


def decode_for_parse(raw: bytes, preferred_encoding: str) -> str:
    encodings: list[str] = []
    if preferred_encoding:
        encodings.append(preferred_encoding)
    system_encoding = locale.getpreferredencoding(False)
    if system_encoding:
        encodings.append(system_encoding)
    encodings.extend(["utf-8", "gb18030"])

    seen: set[str] = set()
    for encoding in encodings:
        normalized = encoding.lower()
        if normalized in seen:
            continue
        seen.add(normalized)
        try:
            text = raw.decode(encoding)
        except UnicodeDecodeError:
            continue
        if "path方向建议" in text or "旋转=" in text:
            return text
    return raw.decode(encodings[0] if encodings else "utf-8", errors="replace")


def angle_to_pulses(angle: float, config: SteeringConfig) -> int:
    if abs(angle) < config.deadzone:
        return 0

    target_angle = abs(angle)
    points = CALIBRATION_POINTS
    for index in range(1, len(points)):
        prev_count, prev_angle = points[index - 1]
        count, measured_angle = points[index]
        if target_angle <= measured_angle:
            ratio = (target_angle - prev_angle) / (measured_angle - prev_angle)
            pulses = round(prev_count + ratio * (count - prev_count))
            return max(1, min(MAX_PULSES, pulses))

    prev_count, prev_angle = points[-2]
    count, measured_angle = points[-1]
    slope = (measured_angle - prev_angle) / (count - prev_count)
    pulses = round(count + (target_angle - measured_angle) / slope)
    return max(1, min(MAX_PULSES, pulses))


def write_dx(serial_port: SerialManager | None, dx: int, config: SteeringConfig) -> None:
    if config.verbose or serial_port is None:
        print(f"[steering] dx={dx}", file=sys.stderr)
    if serial_port is None:
        return

    if config.text:
        payload = f"{dx}\n".encode("ascii")
    else:
        payload = struct.pack("b", dx)
    serial_port.write(payload)


def send_pulses(serial_port: SerialManager | None, angle: float, pulses: int, config: SteeringConfig) -> None:
    if pulses <= 0:
        write_dx(serial_port, 0, config)
        return

    direction = -1 if angle < 0 else 1
    if config.invert:
        direction *= -1
    dx = direction * PULSE_DX

    if config.verbose:
        print(f"[steering] angle={angle:.1f} pulses={pulses} dx={dx}", file=sys.stderr)

    for _ in range(pulses):
        write_dx(serial_port, dx, config)
        time.sleep(PULSE_INTERVAL_SECONDS)
    write_dx(serial_port, 0, config)


def run_child(command: list[str], output: BinaryIO, on_output_line) -> int:
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        stdin=subprocess.DEVNULL,
        bufsize=0,
    )
    assert process.stdout is not None

    try:
        for raw_line in process.stdout:
            output.write(raw_line)
            output.flush()
            on_output_line(raw_line)
        return process.wait()
    except KeyboardInterrupt:
        process.send_signal(signal.SIGTERM)
        try:
            return process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            return process.wait()


def main() -> int:
    command = active_command()
    validate_config(command)

    config = SteeringConfig(
        deadzone=DEADZONE,
        invert=INVERT,
        text=TEXT_PROTOCOL,
        min_interval_ms=max(0, MIN_INTERVAL_MS),
        verbose=VERBOSE,
    )

    serial_port: SerialManager | None = None
    if not DRY_RUN:
        serial_port = SerialManager(SERIAL_PORT, BAUD_RATE)
        serial_port.open_if_needed()

    last_send_time = 0.0

    def handle_line(raw_line: bytes) -> None:
        nonlocal last_send_time
        text = decode_for_parse(raw_line, OUTPUT_ENCODING)
        match = SUGGESTION_RE.search(text)
        if match is None:
            return

        angle = float(match.group(1))
        pulses = angle_to_pulses(angle, config)
        now = time.monotonic()
        if config.min_interval_ms > 0 and (now - last_send_time) * 1000.0 < config.min_interval_ms:
            return
        last_send_time = now

        if config.verbose:
            print(f"[steering] angle={angle:.1f} pulses={pulses}", file=sys.stderr)
        send_pulses(serial_port, angle, pulses, config)

    try:
        return_code = run_child(command, sys.stdout.buffer, handle_line)
    finally:
        if serial_port is not None and SEND_ZERO_ON_EXIT:
            write_dx(serial_port, 0, config)
        if serial_port is not None:
            serial_port.close()

    return return_code


if __name__ == "__main__":
    raise SystemExit(main())
