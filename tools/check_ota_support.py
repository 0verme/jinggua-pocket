"""Verify the pinned StickS3 build has dual OTA slots and boot rollback."""

from __future__ import annotations

import csv
import os
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLATFORMIO_CORE_DIR = Path(
    os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio")
)
FRAMEWORK = PLATFORMIO_CORE_DIR / "packages" / "framework-arduinoespressif32"
PARTITIONS = FRAMEWORK / "tools" / "partitions" / "default_8MB.csv"
SDKCONFIG = (
    FRAMEWORK
    / "tools"
    / "sdk"
    / "esp32s3"
    / "qio_opi"
    / "include"
    / "sdkconfig.h"
)


def read_partitions() -> dict[str, tuple[str, int, int]]:
    result: dict[str, tuple[str, int, int]] = {}
    with PARTITIONS.open(encoding="utf-8", newline="") as stream:
        for row in csv.reader(stream, skipinitialspace=True):
            values = [value.strip() for value in row]
            if not values or values[0].startswith("#"):
                continue
            if len(values) < 5:
                raise SystemExit(f"invalid partition row: {row!r}")
            name, kind, subtype, offset, size = values[:5]
            result[name] = (subtype, int(offset, 0), int(size, 0))
    return result


def main() -> int:
    platformio = (ROOT / "platformio.ini").read_text(encoding="utf-8")
    if "board_build.arduino.partitions = default_8MB.csv" not in platformio:
        raise SystemExit("StickS3 must use the reviewed default_8MB.csv table")
    if "board_build.arduino.memory_type = qio_opi" not in platformio:
        raise SystemExit("StickS3 must keep its reviewed qio_opi memory layout")
    if "platform = espressif32@6.12.0" not in platformio:
        raise SystemExit("OTA rollback check expects the pinned ESP32 platform")
    if "post:tools/ota_config.py" not in platformio:
        raise SystemExit("OTA version/CA injection script is not configured")
    if not PARTITIONS.is_file() or not SDKCONFIG.is_file():
        raise SystemExit("PlatformIO ESP32-S3 framework package is not installed")

    partitions = read_partitions()
    ota_apps = [
        item for item in partitions.values() if item[0] in {"ota_0", "ota_1"}
    ]
    if (
        len(ota_apps) != 2
        or ota_apps[0][2] != ota_apps[1][2]
        or ota_apps[0][2] != 0x330000
    ):
        raise SystemExit("partition table must provide two 0x330000-byte OTA app slots")
    if "otadata" not in partitions or partitions["otadata"][0] != "ota":
        raise SystemExit("partition table must include OTA selection metadata")
    if "nvs" not in partitions or partitions["nvs"][0] != "nvs":
        raise SystemExit("partition table must keep NVS separate from app slots")
    ranges = sorted(
        (offset, offset + size, name)
        for name, (_, offset, size) in partitions.items()
    )
    overlaps = any(
        start < previous_end
        for (_, previous_end, _), (start, _, _) in zip(ranges, ranges[1:])
    )
    if overlaps:
        raise SystemExit("partition table contains overlapping entries")
    if ranges[-1][1] > 0x800000:
        raise SystemExit("partition table exceeds the StickS3 8 MB flash")

    sdkconfig = SDKCONFIG.read_text(encoding="utf-8")
    match = re.search(
        r"^#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE (\d+)$",
        sdkconfig,
        re.MULTILINE,
    )
    if match is None or match.group(1) != "1":
        raise SystemExit("pinned ESP32-S3 bootloader does not enable OTA rollback")

    firmware = ROOT / ".pio" / "build" / "m5stack-sticks3" / "firmware.bin"
    if not firmware.is_file():
        raise SystemExit("firmware image not found; run the StickS3 build first")
    if firmware.stat().st_size > ota_apps[0][2]:
        raise SystemExit(
            f"firmware image ({firmware.stat().st_size} bytes) exceeds OTA slot "
            f"({ota_apps[0][2]} bytes)"
        )
    print(
        "OTA support verified: two equal app slots, isolated NVS, "
        "bootloader rollback enabled"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
