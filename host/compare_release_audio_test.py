#!/usr/bin/env python3
"""Authored alignment/units cases; no game data."""
import unittest
from compare_release_audio import compare


def batch(cycle, frame, value, width=10):
    return [dict(kind='ay', cycle=cycle, frame=frame, beam=beam,
                 reg=reg, value=value) for reg, beam in [(0, 0), (1, width)]]


class ComparisonTests(unittest.TestCase):
    def test_identical(self):
        rows = batch(0, 1, 2) + batch(80000, 2, 3)
        r = compare(rows, rows)
        self.assertEqual(r['span_delta_ms'], [0, 0])
        self.assertEqual(r['equal_board_gap_delta_ms'], [0])

    def test_span_and_equal_cycle_gap(self):
        a = batch(0, 1, 2) + batch(80000, 2, 3)
        b = batch(400000, 10, 2, 20) + batch(480000, 12, 3, 20)
        r = compare(a, b)
        for value in r['span_delta_ms']:
            self.assertAlmostEqual(value, .640)
        self.assertAlmostEqual(r['equal_board_gap_delta_ms'][0], 20)

    def test_changed_interval_not_compared(self):
        a = batch(0, 1, 2) + batch(80000, 2, 3)
        b = batch(0, 1, 2) + batch(80001, 2, 3)
        self.assertEqual(compare(a, b)['equal_board_gap_delta_ms'], [])

    def test_inserted_batch_breaks_adjacent_match(self):
        a = batch(0, 1, 2) + batch(80000, 2, 3)
        b = batch(0, 1, 2) + batch(40000, 2, 99) + batch(80000, 3, 3)
        r = compare(a, b)
        self.assertEqual(len(r['span_delta_ms']), 2)
        self.assertEqual(r['equal_board_gap_delta_ms'], [])

    def test_no_matches_or_audio(self):
        self.assertEqual(compare(batch(0, 1, 2), batch(0, 1, 3))['span_delta_ms'], [])
        self.assertEqual(compare([], [])['span_delta_ms'], [])


if __name__ == '__main__':
    unittest.main()
