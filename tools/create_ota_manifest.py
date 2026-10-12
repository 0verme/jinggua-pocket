"""Create the bounded OTA manifest shipped beside a firmware Release asset."""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path

OTA_SLOT_MAX_BYTES = 0x330000
REPOSITORY = "0verme/jinggua-pocket"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firmware", type=Path, required=True)
    parser.add_argument("--version", required=True, help="numeric SemVer, e.g. 1.0.0")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    version_pattern = (
        r"(?:0|[1-9][0-9]*)\."
        r"(?:0|[1-9][0-9]*)\."
        r"(?:0|[1-9][0-9]*)"
    )
    if not re.fullmatch(version_pattern, args.version):
        parser.error(
            "--version must be numeric major.minor.patch without leading zeroes"
        )
    version_parts = tuple(int(part) for part in args.version.split("."))
    if len(args.version) > 24 or any(part > 0xFFFFFFFF for part in version_parts):
        parser.error("--version exceeds the device parser's bounded numeric range")
    if not args.firmware.is_file():
        parser.error(f"firmware file not found: {args.firmware}")

    image = args.firmware.read_bytes()
    if not image or len(image) > OTA_SLOT_MAX_BYTES:
        parser.error(
            f"firmware size must be 1..{OTA_SLOT_MAX_BYTES} bytes; got {len(image)}"
        )

    digest = hashlib.sha256(image).hexdigest()
    url = (
        f"https://github.com/{REPOSITORY}/releases/download/"
        f"v{args.version}/firmware.bin"
    )
    manifest = (
        "format=1\n"
        f"version={args.version}\n"
        f"size={len(image)}\n"
        f"sha256={digest}\n"
        f"url={url}\n"
    )
    args.output.write_text(manifest, encoding="ascii", newline="\n")
    print(f"created OTA manifest for v{args.version}: {len(image)} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
