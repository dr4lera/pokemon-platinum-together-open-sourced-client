"""Exercise the private merge tool using original synthetic binary fixtures only."""
from dataclasses import replace
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("community_modern_assets", ROOT / "tools/community/prepare-modern-assets.py")
assets = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = assets
spec.loader.exec_module(assets)
NAMES = struct.pack("<4sIIHH", b"BTNF", 16, 4, 0, 1)


def narc(members):
    return assets.encode_narc(assets.Narc(tuple(members), NAMES))


def particle():
    # Original minimal fixture in the supported structural format. No game art.
    resources = bytes(14 * 88)
    texture = struct.pack("<8I", 0, 6, 64, 96, 16, 0, 0, 112) + bytes(80)
    textures = texture * 25
    return struct.pack("<IIHHIIIII", 0x53504120, 0x315f3231, 14, 25, 0,
                       len(resources), len(textures), 32 + len(resources), 0) + resources + textures


def icon():
    raw = struct.pack("<4sHHIHH", b"RGCN", 0xfeff, 0x101, 304, 16, 1)
    raw += struct.pack("<4sI", b"RAHC", 288) + bytes(280)
    compressed = b"\x10" + len(raw).to_bytes(3, "little")
    for offset in range(0, len(raw), 8):
        compressed += b"\0" + raw[offset:offset + 8]
    return compressed


class ModernAssetsTests(unittest.TestCase):
    def test_narc_roundtrip_and_rejections(self):
        good = narc([b"one", b"second", b"three"])
        self.assertEqual(assets.parse_narc(good).members, (b"one", b"second", b"three"))
        mutations = []
        def mutate(offset, value):
            result = bytearray(good)
            result[offset:offset + len(value)] = value
            mutations.append(bytes(result))
        mutate(0, b"BAD!")
        mutate(8, struct.pack("<I", len(good) + 4))
        mutate(16, b"BTNF")
        mutate(20, struct.pack("<I", 0xffffffff))
        mutate(24, b"\0\0")
        mutate(26, b"\1\0")
        mutate(28, struct.pack("<II", 0, 0))
        mutate(36, good[28:36])  # duplicate member bounds
        mutate(36, struct.pack("<I", 2))  # overlap
        mutate(28, struct.pack("<I", 4))  # reordered image
        mutate(32, struct.pack("<I", 0xffffffff))
        fnt = 16 + struct.unpack_from("<I", good, 20)[0]
        mutate(fnt + 14, b"\2\0")
        mutate(fnt + 16, b"BAD!")
        for data in (*mutations, good[:15], good[:-1], good + b"junk"):
            with self.assertRaises(assets.AssetError):
                assets.parse_narc(data)
        with self.assertRaises(assets.AssetError):
            narc([])
        with self.assertRaises(assets.AssetError):
            narc([b""])

    def test_merge_preserves_members_and_requires_pins(self):
        base = assets.parse_narc(narc([b"base0", b"base1"]))
        reference = assets.parse_narc(narc([b"ref0", b"ref1", b"ref2", b"ref3"]))
        pin = lambda index: assets.MemberPin(index, len(reference.members[index]), assets.digest(reference.members[index]))
        merged = assets.merge_members(base, reference, (pin(1),), (pin(2),))
        self.assertEqual(merged.members, (b"base0", b"ref1", b"ref2"))
        failures = [((pin(1), pin(1)), ()), ((pin(1),), (pin(1),)),
                    ((replace(pin(1), sha256="0" * 64),), ()),
                    ((replace(pin(1), size=100),), ()), ((pin(2),), ()), ((), (pin(3),))]
        for replacements, appends in failures:
            with self.assertRaises(assets.AssetError):
                assets.merge_members(base, reference, replacements, appends)

    def test_particle_and_icon_compatibility_bounds(self):
        assets.validate_particle(particle())
        assets.validate_icon(icon())
        for data in (b"", particle()[:-1], bytes(32), particle() + b"tail"):
            with self.assertRaises(assets.AssetError):
                assets.validate_particle(data)
        wrong = bytearray(particle())
        struct.pack_into("<I", wrong, 32, 15)  # unsupported emission type
        with self.assertRaises(assets.AssetError):
            assets.validate_particle(wrong)
        for data in (b"", b"\x10\x30\x01\0\x80\0\0", icon()[:-1], icon() + b"extra"):
            with self.assertRaises(assets.AssetError):
                assets.validate_icon(data)

    def test_private_preflight_output_and_whole_hashes(self):
        # The tool intentionally refuses checkout-contained input/output. Keep
        # disposable synthetic fixtures in a fresh sibling directory instead.
        with tempfile.TemporaryDirectory(prefix="community-modern-assets-tests-", dir=ROOT.parent) as directory:
            temp = Path(directory)
            self.assertTrue(temp.resolve().is_relative_to(ROOT.parent.resolve()))
            base, reference, output = [temp / name for name in ("base", "reference", "merged")]
            original_policy = assets.ASSETS
            fixtures, policies = {}, []
            for policy in original_policy:
                base_members = [b"B" + struct.pack("<H", i) for i in range(policy.base_count)]
                reference_members = [b"R" + struct.pack("<H", i) for i in range(policy.reference_count)]
                if "particle" in policy.path:
                    reference_members[485] = particle()
                if "pl_batt_obj" in policy.path:
                    reference_members[343] = icon()
                base_data, reference_data = narc(base_members), narc(reference_members)
                for root, relative, data in ((base, policy.path, base_data),
                                             (reference, "patch/" + policy.path, reference_data)):
                    path = root / relative
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(data)
                    fixtures[path] = data
                def member(pin):
                    data = reference_members[pin.index]
                    return replace(pin, size=len(data), sha256=assets.digest(data))
                policies.append(replace(policy, base_sha256=assets.digest(base_data),
                    reference_sha256=assets.digest(reference_data),
                    replace=tuple(member(p) for p in policy.replace), append=tuple(member(p) for p in policy.append)))
            # Production has fixed pins and no CLI override. Rebind only this
            # imported test module's policy to independent synthetic fixtures.
            assets.ASSETS = tuple(policies)
            try:
                manifest = assets.prepare(output, base, reference)
                self.assertTrue(manifest["private"])
                self.assertFalse(manifest["runtime_verified"])
                self.assertEqual(len(manifest["files"]), 3)
                for policy, record in zip(policies, manifest["files"]):
                    incoming = assets.parse_narc((base / policy.path).read_bytes())
                    merged = assets.parse_narc((output / policy.path).read_bytes())
                    selected = {p.index for p in policy.replace}
                    self.assertEqual(len(merged.members), policy.base_count + len(policy.append))
                    for i, value in enumerate(incoming.members):
                        if i not in selected:
                            self.assertEqual(merged.members[i], value)
                    self.assertEqual(record["sha256"], assets.digest((output / policy.path).read_bytes()))
                for path, data in fixtures.items():
                    self.assertEqual(path.read_bytes(), data)
                self.assertNotIn(str(base), (output / "PRIVATE-MODERN-ASSETS.json").read_text())
                for forbidden in (output, base / "patch", reference / "new", ROOT / ".forbidden-modern-assets-test"):
                    with self.assertRaises(assets.AssetError):
                        assets.prepare(forbidden, base, reference)
                self.assertFalse((base / "patch").exists())
                # Changing even an unselected reference member must fail the
                # whole-archive pin, before creating any output directory.
                path = reference / "patch" / policies[-1].path
                changed = bytearray(path.read_bytes())
                changed[-1] ^= 1
                path.write_bytes(changed)
                refused = temp / "refused"
                with self.assertRaisesRegex(assets.AssetError, "Whole archive hash"):
                    assets.prepare(refused, base, reference)
                self.assertFalse(refused.exists())
                path.write_bytes(fixtures[path])
                with self.assertRaises(assets.AssetError):
                    assets.read_archive(base, "../reference/patch/" + policies[-1].path,
                                        policies[-1].reference_count, policies[-1].reference_sha256)
                try:
                    link = temp / "output-link"
                    link.symlink_to(temp / "nonexistent", target_is_directory=True)
                except OSError:
                    print("Output symlink fixture unavailable on this host")
                else:
                    with self.assertRaises(assets.AssetError):
                        assets.prepare(link, base, reference)
                    self.assertFalse((temp / "nonexistent").exists())
            finally:
                assets.ASSETS = original_policy
            # The shipped policy cannot accept these synthetic archive hashes.
            with self.assertRaisesRegex(assets.AssetError, "Whole archive hash"):
                assets.prepare(temp / "real-policy-refusal", base, reference)


if __name__ == "__main__":
    unittest.main()
