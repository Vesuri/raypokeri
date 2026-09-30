#!/usr/bin/env python3
import struct
import unittest
from exit_trace import MARKER, inspect


def words(*values):
    return struct.pack('<' + 'I' * len(values), *values)


def capture(marker=True, recorded_pc=False, cost=100, claimed=100):
    registers = [0] * 17
    if marker:
        registers[6:8] = MARKER
    record = (words(0x1234) if recorded_pc else b'') + words(0xffffffff-cost, *registers)
    header = words(1, 0) + bytes(16) + words(0, 0, 0, 28375160, 128)
    frame = words(520) + bytes(520) + words(0, 0, 0, 0, 0)
    frame += words(claimed, 0, len(record)//4) + record + words(0, 0)
    return header + frame


class TraceTests(unittest.TestCase):
    def test_unrecorded_pc_retains_register_marker(self):
        result = inspect(capture())
        self.assertEqual(len(result['markers']), 1)
        self.assertEqual(result['markers'][0]['pal_fields_from_capture'], 0)
        self.assertEqual(result['total_cycles'], 100)

    def test_recorded_pc(self):
        self.assertEqual(len(inspect(capture(recorded_pc=True))['markers']), 1)

    def test_absent(self):
        self.assertEqual(inspect(capture(marker=False))['markers'], [])

    def test_truncation(self):
        data = capture()
        for length in (0, 7, 30, len(data)-1, len(data)-20):
            with self.subTest(length=length), self.assertRaises(ValueError):
                inspect(data[:length])

    def test_trailing(self):
        with self.assertRaisesRegex(ValueError, 'trailing'):
            inspect(capture() + bytes(4))

    def test_accounting(self):
        with self.assertRaisesRegex(ValueError, 'account'):
            inspect(capture(claimed=1000))

    def test_zero_cycles(self):
        with self.assertRaisesRegex(ValueError, 'zero-cycle'):
            inspect(capture(claimed=0))


if __name__ == '__main__':
    unittest.main()
