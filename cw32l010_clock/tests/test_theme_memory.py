"""Check the design-only asset codec and shared-resource capacity calculation."""
import itertools
from pathlib import Path
import random
import sys
import unittest

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import theme_memory as memory


class BitmapTest(unittest.TestCase):
    def round_trip(self, image):
        encoded = memory.encode_bitmap(image)
        decoded = memory.decode_bitmap(encoded)
        self.assertEqual(decoded.size, image.size)
        self.assertEqual(decoded.tobytes(), image.tobytes())
        return encoded

    def test_tiny_raw_bitmap(self):
        image = Image.new("1", (3, 5), 1)
        image.putpixel((1, 2), 0)
        self.assertEqual(self.round_trip(image)[0], 0)

    def test_single_byte_dictionary_with_partial_tiles(self):
        image = Image.new("1", (297, 153), 1)
        self.assertEqual(self.round_trip(image)[0], 1)

    def test_two_byte_dictionary(self):
        random_bytes = random.Random(20261020).randbytes
        tiles = [random_bytes(8) for _ in range(300)]
        image = Image.new("1", (2400, 64), 0)
        for row in range(8):
            for col, value in enumerate(tiles):
                image.paste(Image.frombytes("1", (8, 8), value), (col * 8, row * 8))
        self.assertEqual(self.round_trip(image)[0], 2)

    def test_noise_falls_back_to_raw(self):
        image = Image.frombytes("1", (296, 152), random.Random(20).randbytes(5624))
        self.assertEqual(self.round_trip(image)[0], 0)


class CapacityTest(unittest.TestCase):
    def test_shared_assets_match_brute_force(self):
        assets = {name: memory.Asset(name, bytes(size)) for name, size in
                  (("shared", 100), ("a", 200), ("b", 300), ("c", 400), ("other", 160))}
        rows = [{"number": 1, "asset_keys": ["shared", "a"]},
                {"number": 2, "asset_keys": ["shared", "b"]},
                {"number": 3, "asset_keys": ["other", "c"]}]
        for budget in (0, 256, 556, 1100, 1500, 10000):
            result = memory.subset_budget(rows, assets, budget)
            costs = []
            for count in range(4):
                for selection in itertools.combinations(rows, count):
                    keys = {key for row in selection for key in row["asset_keys"]}
                    size = sum(assets[key].size for key in keys) + memory.THEME_CODE_ALLOWANCE * count
                    costs.append((count, size))
            expected = max(count for count, size in costs if size <= budget)
            self.assertEqual(result["max_new_themes_under_model"], expected)
            self.assertEqual(result["selected_assets_and_theme_allowances_bytes"],
                             min(size for count, size in costs if count == expected))
            guaranteed = max(count for count in range(4)
                             if max(size for n, size in costs if n == count) <= budget)
            self.assertEqual(result["any_selection_new_theme_count_under_model"], guaranteed)


if __name__ == "__main__":
    unittest.main()
