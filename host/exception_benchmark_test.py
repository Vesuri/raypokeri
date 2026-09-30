#!/usr/bin/env python3
"""Reject incomplete/overflowed/miscounted forwarding measurements."""
import struct
import tempfile
import unittest
from pathlib import Path
from exception_benchmark import read


class Records(unittest.TestCase):
    def test_records(self):
        good = b"PKEX0001" + struct.pack(">4I", 709379, 128, 4, 5)
        good += struct.pack(">3I", 100, 1000, 128) * 20
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "record"
            path.write_bytes(good)
            values = read(path)
            self.assertEqual(len(values), 5)
            self.assertTrue(all(len(v) == 4 for v in values.values()))
            self.assertAlmostEqual(values["Line-A"][0], 900e6/709379/128)
            invalid = [good[:-1], good+b"x", b"BAD!"+good[4:]]
            for offset, value in ((8, 1), (12, 0), (16, 3), (20, 6),
                                  (24, 0), (28, 100), (28, 65536), (32, 127)):
                row = bytearray(good)
                struct.pack_into(">I", row, offset, value)
                invalid.append(row)
            for data in invalid:
                path.write_bytes(data)
                with self.assertRaises(ValueError):
                    read(path)


if __name__ == "__main__":
    unittest.main()
