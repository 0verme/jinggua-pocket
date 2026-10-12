import struct
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from merge_font_assets import merge_payload, unpack_payload  # noqa: E402


def _payload(glyphs):
    header = struct.pack(">6I", len(glyphs), 11, 16, 0, 12, 4)
    records = b"".join(
        struct.pack(">7I", codepoint, 1, 1, 1, 1, 0, 0)
        for codepoint, _ in glyphs
    )
    bitmaps = b"".join(bitmap for _, bitmap in glyphs)
    return header + records + bitmaps


class FontAssetMergeTests(unittest.TestCase):
    def test_existing_glyph_bytes_are_preserved_and_missing_are_added(self):
        existing = _payload([(ord("A"), b"\xAA")])
        generated = _payload(
            [(ord("A"), b"\x11"), (ord("中"), b"\x22")]
        )

        merged, added = merge_payload(existing, generated)
        header, glyphs = unpack_payload(merged)

        self.assertEqual(added, 1)
        self.assertEqual(header[0], 2)
        self.assertEqual(glyphs[ord("A")][1], b"\xAA")
        self.assertEqual(glyphs[ord("中")][1], b"\x22")
        self.assertEqual(list(glyphs), [ord("A"), ord("中")])

    def test_refuses_incompatible_font_metrics(self):
        existing = _payload([(ord("A"), b"\xAA")])
        incompatible = struct.pack(">6I", 1, 11, 18, 0, 14, 4)
        incompatible += struct.pack(">7I", ord("B"), 1, 1, 1, 1, 0, 0)
        incompatible += b"\xBB"

        with self.assertRaises(ValueError):
            merge_payload(existing, incompatible)


if __name__ == "__main__":
    unittest.main()
