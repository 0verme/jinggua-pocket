"""Check that the committed VLW asset contains every firmware glyph."""

from __future__ import annotations

import re
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ASCII_GLYPHS = set(range(0x20, 0x7F))


def source_codepoints() -> set[int]:
    codepoints = set(ASCII_GLYPHS)
    for source_dir in (ROOT / "src", ROOT / "include"):
        for path in source_dir.rglob("*"):
            if path.suffix in {".cpp", ".h", ".hpp"}:
                codepoints.update(ord(character) for character in path.read_text(encoding="utf-8"))
    return {codepoint for codepoint in codepoints if codepoint <= 0xFFFF}


def read_asset() -> bytes:
    source = (ROOT / "src" / "hardware" / "font_data.cpp").read_text(encoding="utf-8")
    return bytes(int(value, 16) for value in re.findall(r"0x([0-9A-F]{2})", source))


def main() -> int:
    payload = read_asset()
    if len(payload) < 24:
        raise SystemExit("font asset is shorter than the VLW header")
    count, version, _line_height, _reserved, _ascent, _descent = struct.unpack(">6I", payload[:24])
    if version != 11:
        raise SystemExit(f"unexpected VLW version: {version}")
    records_end = 24 + count * 28
    if len(payload) < records_end:
        raise SystemExit("font asset is shorter than its glyph table")

    glyphs: list[int] = []
    bitmap_end = records_end
    for index in range(count):
        offset = 24 + index * 28
        codepoint, height, width, _advance, _y, _x, _reserved = struct.unpack(
            ">7I", payload[offset : offset + 28]
        )
        glyphs.append(codepoint)
        bitmap_end += width * height
    if bitmap_end != len(payload):
        raise SystemExit(f"VLW bitmap size mismatch: table expects {bitmap_end}, file has {len(payload)}")
    if glyphs != sorted(set(glyphs)):
        raise SystemExit("VLW glyph table must contain unique, sorted codepoints")

    expected = source_codepoints()
    missing = expected.difference(glyphs)
    if missing:
        missing_text = ", ".join(f"U+{codepoint:04X}" for codepoint in sorted(missing))
        raise SystemExit(f"font asset is missing source glyphs: {missing_text}")
    print(f"font asset checks passed: {count} glyphs, {len(payload)} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
