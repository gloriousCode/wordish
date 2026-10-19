"""Verify real generated font coverage, metrics and platform isolation."""
import json
from pathlib import Path
import struct
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read_glyphs(path):
    data = path.read_bytes()
    version, height, count, wildcard, buckets, cp_bytes, header_size, features = struct.unpack_from('<BBHHBBBB', data)
    assert version == 3 and cp_bytes == 2 and features in (0, 1)
    offsets = header_size + buckets * 4
    entry_size = 4 if features & 1 else 6
    glyph_base = offsets + count * entry_size
    glyphs = {}
    for index in range(count):
        cp, offset = struct.unpack_from('<HH' if features & 1 else '<HI', data, offsets + index * entry_size)
        start = glyph_base + offset
        metrics = struct.unpack_from('<BBbbb', data, start)
        width, rows = metrics[:2]
        length = ((width * rows + 31) // 32) * 4
        glyphs[cp] = (metrics, data[start + 5:start + 5 + length])
    return glyphs


class SmoothFontTests(unittest.TestCase):
    def test_nested_masks_have_identical_layout_metrics(self):
        for size in (48, 54):
            for style in ('Condensed', 'Bold'):
                with self.subTest(size=size, style=style):
                    masks = [read_glyphs(ROOT / f'resources/fonts/smooth/Roboto-{style}-{size}-{level}.pbf') for level in (1, 2, 3)]
                    self.assertGreater(len(masks[0]), 100)
                    for cp, (metrics, outer) in masks[0].items():
                        middle_metrics, middle = masks[1][cp]
                        inner_metrics, inner = masks[2][cp]
                        self.assertEqual(metrics, middle_metrics)
                        self.assertEqual(metrics, inner_metrics)
                        self.assertEqual(len(outer), len(inner))
                        self.assertTrue(all((c & ~b) == 0 and (b & ~a) == 0 for a, b, c in zip(outer, middle, inner)))
                        width, rows, left, top, advance = metrics
                        self.assertLessEqual(top + rows, size + 12)
                    # Curves really contain intermediate shades, not three identical masks.
                    self.assertNotEqual(masks[0][ord('o')][1], masks[1][ord('o')][1])
                    self.assertNotEqual(masks[1][ord('o')][1], masks[2][ord('o')][1])

    def test_resources_only_target_modern_colour_platforms(self):
        media = json.loads((ROOT / 'appinfo.json').read_text())['resources']['media']
        masks = [entry for entry in media if entry['name'].startswith('FONT_SMOOTH_')]
        self.assertEqual(len(masks), 12)
        for entry in masks:
            self.assertEqual(entry['targetPlatforms'], ['emery', 'gabbro'])
            self.assertTrue((ROOT / 'resources' / entry['file']).is_file())


if __name__ == '__main__':
    unittest.main()
