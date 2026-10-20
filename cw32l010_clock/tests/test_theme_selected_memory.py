"""Verify bounded, lossless compression used by the replacement-theme audit."""
from pathlib import Path
import random
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import theme_selected_memory as selected


class CompressionTest(unittest.TestCase):
    def test_packbits_run_and_literal_boundaries(self):
        for length in (0, 1, 2, 3, 127, 128, 129, 255, 256, 257):
            for value in (bytes(length), b"x" * length,
                          bytes(i % 256 for i in range(length)),
                          (b"ab" * length)[:length]):
                self.assertEqual(selected.runs_decode(selected.runs_encode(value)), value)

    def test_random_packbits_inputs(self):
        random_bytes = random.Random(20261020).randbytes
        for length in range(520):
            raw = random_bytes(length)
            self.assertEqual(selected.runs_decode(selected.runs_encode(raw)), raw)

    def test_prediction_is_invertible(self):
        raw = random.Random(20).randbytes(128)
        for stride in (0, 1, 7, 19, 37, 64, 127):
            self.assertEqual(selected.undo_difference(selected.difference(raw, stride), stride), raw)

    def test_random_access_blocks_round_trip(self):
        random_bytes = random.Random(128).randbytes
        for length in (0, 1, 127, 128, 129, 256, 4096, 5624):
            for raw in (random_bytes(length), bytes(length), (b"ABC" * length)[:length]):
                for row_bytes in (1, 7, 19, 37):
                    self.assertEqual(selected.block_decode(selected.block_encode(raw, row_bytes)), raw)

    def test_all_candidate_block_boundaries(self):
        random_bytes = random.Random(512).randbytes
        for size in (128, 256, 512):
            with patch.object(selected, "BLOCK_BYTES", size):
                for length in (size - 1, size, size + 1, 2 * size + 1):
                    for raw in (random_bytes(length), (b"ABCDE" * length)[:length]):
                        encoded = selected.block_encode(raw, 19)
                        self.assertEqual(selected.block_decode(encoded), raw)


if __name__ == "__main__":
    unittest.main()
