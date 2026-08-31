"""Generate a small anti-aliased VLW font subset for the StickS3 firmware.

The source font is intentionally supplied by the caller instead of committed to
the repository.  This keeps the checkout small while making the generated
firmware asset reproducible from a licensed Noto Sans SC installation.
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


DEFAULT_FONT = Path(r"C:\Windows\Fonts\NotoSansSC-VF.ttf")
SOURCE_EXTENSIONS = {".cpp", ".h", ".hpp"}
ASCII_GLYPHS = "".join(chr(codepoint) for codepoint in range(0x20, 0x7F))


def collect_codepoints(source_root: Path) -> list[int]:
    """Collect ASCII plus every non-ASCII character used by firmware sources."""

    characters = set(ASCII_GLYPHS)
    for source_dir in (source_root / "src", source_root / "include"):
        if not source_dir.is_dir():
            continue
        for path in source_dir.rglob("*"):
            if path.suffix not in SOURCE_EXTENSIONS:
                continue
            characters.update(path.read_text(encoding="utf-8"))
    return sorted(ord(character) for character in characters if ord(character) <= 0xFFFF)


def load_font(path: Path, pixel_size: int, weight: int) -> ImageFont.FreeTypeFont:
    font = ImageFont.truetype(str(path), pixel_size)
    axes = getattr(font, "get_variation_axes", lambda: [])()
    if axes:
        if len(axes) != 1 or axes[0]["name"] != b"Weight":
            raise ValueError("expected a single Weight axis in the Noto Sans SC font")
        font.set_variation_by_axes([weight])
    return font


def rasterize(font: ImageFont.FreeTypeFont, codepoints: list[int]) -> tuple[bytes, int, int, int]:
    glyphs: list[tuple[int, int, int, int, int, int, bytes]] = []
    max_ascent = 0
    max_descent = 0

    for codepoint in codepoints:
        character = chr(codepoint)
        bbox = font.getbbox(character, anchor="ls")
        if bbox is None:
            raise ValueError(f"font does not contain U+{codepoint:04X}")
        left, top, right, bottom = bbox
        width = max(0, right - left)
        height = max(0, bottom - top)
        ascent = max(0, -top)
        descent = max(0, bottom)
        max_ascent = max(max_ascent, ascent)
        max_descent = max(max_descent, descent)
        advance = round(font.getlength(character))

        bitmap_data = b""
        if width and height:
            bitmap = Image.new("L", (width, height), 0)
            ImageDraw.Draw(bitmap).text(
                (-left, ascent), character, font=font, fill=255, anchor="ls"
            )
            bitmap_data = bytes(
                bitmap.get_flattened_data()
                if hasattr(bitmap, "get_flattened_data")
                else bitmap.getdata()
            )
        glyphs.append(
            (codepoint, height, width, advance, ascent, left, bytes(bitmap_data))
        )

    if len(glyphs) > 0xFFFF:
        raise ValueError("VLW glyph count exceeds the format limit")
    if max_ascent + max_descent > 0xFFFF:
        raise ValueError("VLW line height exceeds the format limit")

    payload = bytearray()
    payload.extend(struct.pack(">6I", len(glyphs), 11, max_ascent + max_descent, 0,
                               max_ascent, max_descent))
    for codepoint, height, width, advance, ascent, left, _ in glyphs:
        if any(value > 0xFF for value in (height, width, advance)):
            raise ValueError(f"glyph U+{codepoint:04X} exceeds VLW metric range")
        if not -128 <= left <= 127:
            raise ValueError(f"glyph U+{codepoint:04X} has an unsupported x offset")
        payload.extend(struct.pack(">7I", codepoint, height, width, advance,
                                   ascent, left & 0xFF, 0))
    for _, _, _, _, _, _, bitmap in glyphs:
        payload.extend(bitmap)
    return bytes(payload), len(glyphs), max_ascent, max_descent


def write_header(path: Path, glyph_count: int, byte_count: int, pixel_size: int, weight: int) -> None:
    path.write_text(
        "#pragma once\n\n"
        "#include <cstddef>\n"
        "#include <cstdint>\n\n"
        "namespace jinggua::hardware {\n\n"
        "extern const std::uint8_t kChineseFontData[];\n"
        "extern const std::size_t kChineseFontDataSize;\n\n"
        f"constexpr std::uint16_t kChineseFontGlyphCount = {glyph_count};\n"
        f"constexpr std::size_t kChineseFontDataBytes = {byte_count};\n"
        f"constexpr std::uint8_t kChineseFontPixelSize = {pixel_size};\n"
        f"constexpr std::uint16_t kChineseFontWeight = {weight};\n\n"
        "}  // namespace jinggua::hardware\n",
        encoding="utf-8",
        newline="\n",
    )


def write_source(path: Path, payload: bytes) -> None:
    values = [f"0x{value:02X}" for value in payload]
    lines = [
        '#include "jinggua/hardware/font_data.h"',
        "",
        "namespace jinggua::hardware {",
        "",
        "alignas(4) const std::uint8_t kChineseFontData[] = {",
    ]
    lines.extend(
        f"  {', '.join(values[index:index + 16])},"
        for index in range(0, len(values), 16)
    )
    lines.extend([
        "};",
        "",
        "const std::size_t kChineseFontDataSize = sizeof(kChineseFontData);",
        "",
        "}  // namespace jinggua::hardware",
        "",
    ])
    path.write_text("\n".join(lines), encoding="utf-8", newline="\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", type=Path, default=DEFAULT_FONT)
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output-dir", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--pixel-size", type=int, default=12)
    parser.add_argument("--weight", type=int, default=500)
    args = parser.parse_args()

    if not args.font.is_file():
        parser.error(f"font file not found: {args.font}")
    if not 1 <= args.pixel_size <= 255:
        parser.error("--pixel-size must be between 1 and 255")
    if not 1 <= args.weight <= 1000:
        parser.error("--weight must be between 1 and 1000")

    codepoints = collect_codepoints(args.source_root)
    font = load_font(args.font, args.pixel_size, args.weight)
    payload, glyph_count, ascent, descent = rasterize(font, codepoints)

    include_dir = args.output_dir / "include" / "jinggua" / "hardware"
    source_dir = args.output_dir / "src" / "hardware"
    include_dir.mkdir(parents=True, exist_ok=True)
    source_dir.mkdir(parents=True, exist_ok=True)
    write_header(include_dir / "font_data.h", glyph_count, len(payload), args.pixel_size, args.weight)
    write_source(source_dir / "font_data.cpp", payload)
    print(
        f"generated {glyph_count} glyphs, {len(payload)} bytes, "
        f"{args.pixel_size}px/{args.weight} weight, ascent={ascent}, descent={descent}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
