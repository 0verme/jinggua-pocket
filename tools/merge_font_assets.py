"""Add source glyphs while preserving already-rendered VLW glyph bitmaps.

This avoids changing the appearance of all existing Chinese text when a newer
Pillow/FreeType build or compatible Noto Sans SC file produces slightly
different rasterization bytes.
"""

from __future__ import annotations

import argparse
import re
import struct
from pathlib import Path

VLW_HEADER_BYTES = 24
VLW_GLYPH_BYTES = 28


def read_source_payload(path: Path) -> bytes:
    values = re.findall(r"0x([0-9A-Fa-f]{2})", path.read_text(encoding="utf-8"))
    if not values:
        raise ValueError(f"no VLW bytes found in {path}")
    return bytes(int(value, 16) for value in values)


def unpack_payload(
    payload: bytes,
) -> tuple[tuple[int, ...], dict[int, tuple[bytes, bytes]]]:
    if len(payload) < VLW_HEADER_BYTES:
        raise ValueError("VLW payload is shorter than its header")
    header = struct.unpack(">6I", payload[:VLW_HEADER_BYTES])
    glyph_count = header[0]
    bitmap_offset = VLW_HEADER_BYTES + glyph_count * VLW_GLYPH_BYTES
    if bitmap_offset > len(payload):
        raise ValueError("VLW glyph table extends beyond the payload")

    glyphs: dict[int, tuple[bytes, bytes]] = {}
    cursor = bitmap_offset
    for index in range(glyph_count):
        start = VLW_HEADER_BYTES + index * VLW_GLYPH_BYTES
        record = payload[start : start + VLW_GLYPH_BYTES]
        codepoint, height, width = struct.unpack(">3I", record[:12])
        bitmap_size = height * width
        end = cursor + bitmap_size
        if end > len(payload) or codepoint in glyphs:
            raise ValueError("invalid VLW bitmap table")
        glyphs[codepoint] = (record, payload[cursor:end])
        cursor = end
    if cursor != len(payload):
        raise ValueError("unexpected bytes after VLW glyph bitmaps")
    return header, glyphs


def merge_payload(existing: bytes, generated: bytes) -> tuple[bytes, int]:
    old_header, old_glyphs = unpack_payload(existing)
    new_header, new_glyphs = unpack_payload(generated)
    if old_header[1:] != new_header[1:]:
        raise ValueError("font metrics differ; refusing to change existing glyphs")

    additions = {
        key: value for key, value in new_glyphs.items() if key not in old_glyphs
    }
    glyphs = dict(old_glyphs)
    glyphs.update(additions)

    payload = bytearray(
        struct.pack(
            ">6I",
            len(glyphs),
            old_header[1],
            old_header[2],
            old_header[3],
            old_header[4],
            old_header[5],
        )
    )
    for codepoint in sorted(glyphs):
        payload.extend(glyphs[codepoint][0])
    for codepoint in sorted(glyphs):
        payload.extend(glyphs[codepoint][1])
    return bytes(payload), len(additions)


def main() -> int:
    from generate_font_assets import (
        collect_codepoints,
        load_font,
        rasterize,
        write_header,
        write_source,
    )

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", type=Path, required=True)
    parser.add_argument(
        "--source-root", type=Path, default=Path(__file__).resolve().parents[1]
    )
    parser.add_argument(
        "--output-dir", type=Path, default=Path(__file__).resolve().parents[1]
    )
    parser.add_argument("--existing-source", type=Path)
    parser.add_argument("--pixel-size", type=int, default=12)
    parser.add_argument("--weight", type=int, default=500)
    args = parser.parse_args()

    source_root = args.source_root.resolve()
    output_dir = args.output_dir.resolve()
    existing_source = args.existing_source or (
        output_dir / "src" / "hardware" / "font_data.cpp"
    )
    codepoints = collect_codepoints(source_root)
    font = load_font(args.font, args.pixel_size, args.weight)
    generated, _, ascent, descent = rasterize(font, codepoints)

    if existing_source.is_file():
        existing = read_source_payload(existing_source)
        merged, added = merge_payload(existing, generated)
    else:
        merged = generated
        added = len(codepoints)

    include_dir = output_dir / "include" / "jinggua" / "hardware"
    source_dir = output_dir / "src" / "hardware"
    include_dir.mkdir(parents=True, exist_ok=True)
    source_dir.mkdir(parents=True, exist_ok=True)
    glyph_count = struct.unpack(">I", merged[:4])[0]
    write_header(
        include_dir / "font_data.h",
        glyph_count,
        len(merged),
        args.pixel_size,
        args.weight,
    )
    write_source(source_dir / "font_data.cpp", merged)
    print(
        f"merged {added} new glyphs; {glyph_count} total, {len(merged)} bytes, "
        f"ascent={ascent}, descent={descent}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
