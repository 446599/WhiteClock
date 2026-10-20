"""Compare MCU resource decoding and glyph dilation against independent bitmaps."""
import ctypes
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "tests/fixtures/theme-assets"


class ResourceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="clock-assets-test-")
        library = Path(cls.temp.name) / "clock-theme.dylib"
        subprocess.run(["cc", "-std=c11", "-O2", "-shared", "-fPIC", "-DCLOCK_THEME_TEST", "-Iuser/inc",
                        "user/src/clock_core.c", "user/src/clock_render.c", "user/src/clock_theme.c",
                        "-o", str(library)], cwd=ROOT, check=True)
        cls.lib = ctypes.CDLL(str(library))
        cls.lib.clock_theme_asset_pixel.argtypes = [ctypes.c_uint] * 3
        cls.lib.clock_theme_asset_pixel.restype = ctypes.c_bool
        cls.lib.clock_theme_mask.argtypes = [ctypes.c_uint] * 3
        cls.lib.clock_theme_mask.restype = ctypes.c_uint64
        cls.images = {}
        for item in json.loads((ASSETS / "manifest.json").read_text()):
            with Image.open(ASSETS / f"{item['id']:02d}-{item['name']}.png") as source:
                image = source.copy()
            if list(image.size) != item["size"] or hashlib.sha256(image.tobytes()).hexdigest() != item["sha256"]:
                raise AssertionError(f"Reference image integrity failure: {item['name']}")
            cls.images[item["id"]] = image

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_every_native_asset_pixel(self):
        for index, image in self.images.items():
            pixels = image.load()
            for x in range(image.width):
                for y in range(image.height):
                    actual = self.lib.clock_theme_asset_pixel(index, x, y)
                    if actual != bool(pixels[x, y]):
                        self.fail(f"asset={index} x={x} y={y}")
            self.assertFalse(self.lib.clock_theme_asset_pixel(index, image.width, 0))
            self.assertFalse(self.lib.clock_theme_asset_pixel(index, 0, image.height))

    def test_every_glyph_column_and_halo(self):
        for font in json.loads((ASSETS / "fonts.json").read_text()):
            image = self.images[font["image"]].convert("L")
            for index, metrics in enumerate(font["glyphs"]):
                _, width, height, *_ = metrics
                x = index % font["columns"] * font["cell_w"]
                y = index // font["columns"] * font["cell_h"]
                glyph = image.crop((x, y, x + width, y + height))
                for halo in range(3):
                    expected = Image.new("L", (width + 2 * halo, height + 2 * halo), 0)
                    expected.paste(glyph, (halo, halo))
                    if halo:
                        expected = expected.filter(ImageFilter.MaxFilter(halo * 2 + 1))
                    for col in range(expected.width):
                        bits = sum((1 << row) for row in range(expected.height) if expected.getpixel((col, row)))
                        self.assertEqual(self.lib.clock_theme_mask(font["id"], index | (halo << 8), col), bits)


if __name__ == "__main__":
    unittest.main()
