# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic encoded-bank bounds and exact selective merge checks; no game files."""
import importlib.util
from pathlib import Path
import struct
import sys
import unittest

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("species_text", root / "tools/community/prepare-species-text.py")
text = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = text
spec.loader.exec_module(text)


def encode(entries, seed=19):
    table, payload = bytearray(), bytearray()
    offset = 4 + 8 * len(entries)
    for i, words in enumerate(entries):
        key = seed * 765 * (i + 1) & 65535
        key |= key << 16
        table += struct.pack("<II", offset ^ key, len(words) ^ key)
        payload += struct.pack("<" + "H" * len(words), *(v ^ (((i + 1) * 596947 + j * 18749) & 65535) for j, v in enumerate(words)))
        offset += 2 * len(words)
    return struct.pack("<HH", len(entries), seed) + table + payload


class TextChecks(unittest.TestCase):
    def test_decoded_prefix_and_unselected_preservation(self):
        original = ((101, 65535), (102, 103, 65535))
        extension = (*original, (104, 65535))
        base, modern = encode(original), encode(extension, seed=271)
        self.assertEqual(text.decode_bank(base), list(original))
        pins = ((412, 2, text.assets.digest(base), text.assets.digest(modern), 3),)
        a = text.assets.Narc(tuple(base for _ in range(724)), struct.pack("<4sIIHH", b"BTNF", 16, 4, 0, 1))
        b = text.assets.Narc(tuple(modern for _ in range(724)), a.names)
        result = text.merge_banks(a, b, pins, reference_count=3)
        self.assertEqual(result.members[412], modern)
        self.assertTrue(all(result.members[i] == base for i in range(724) if i != 412))
        self.assertEqual(text.assets.parse_narc(text.assets.encode_narc(result)), result)
        changed = encode(((999, 65535), *extension[1:]), seed=271)
        b = text.assets.Narc(tuple(changed for _ in range(724)), a.names)
        altered_pins = ((412, 2, text.assets.digest(base), text.assets.digest(changed), 3),)
        with self.assertRaises(text.assets.AssetError):
            text.merge_banks(a, b, altered_pins, reference_count=3)
        with self.assertRaises(text.assets.AssetError):
            text.merge_banks(a, b, pins, reference_count=3)

    def test_truncation_overlaps_terminators_and_limits(self):
        valid = encode(((5, 65535), (6, 65535)))
        for size in range(len(valid)):
            with self.assertRaises(text.assets.AssetError):
                text.decode_bank(valid[:size])
        for invalid in (valid + b"\0\0", encode(((5, 7),)), encode(((65535,),) * 2049)):
            with self.assertRaises(text.assets.AssetError):
                text.decode_bank(invalid)
        with self.assertRaises(text.assets.AssetError):
            text.decode_bank(valid, maximum=1)
        # Force the second entry to overlap the first, retaining valid encryption.
        invalid = bytearray(valid)
        key = (19 * 765 * 2) & 65535
        key |= key << 16
        struct.pack_into("<I", invalid, 12, 20 ^ key)
        with self.assertRaises(text.assets.AssetError):
            text.decode_bank(invalid)


if __name__ == "__main__":
    unittest.main()
