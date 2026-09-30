#!/usr/bin/env python3
"""Adversarial checks for the live sound stream verifier."""
import unittest
from sound_trace_check import check

class StreamTests(unittest.TestCase):
    def test_mask_and_unrelated_pia_bits(self):
        result = check('G 1 22 255\nP 1 15\nR\nG 7 2 255\nP 7 255\nR')
        self.assertEqual(result[:3], (2, 0, 0))
        self.assertEqual(result[3], result[4])

    def test_capture_edges_are_explicit(self):
        self.assertEqual(check('P 7 255\nR\nG 1 2 15')[:3], (0, 1, 1))

    def test_bad_streams_fail(self):
        for events in (
            'G 1 2 15\nR',                         # lost write
            'G 1 2 15\nP 2 15',                    # wrong register
            'G 1 2 15\nP 1 14',                    # wrong value
            'G 1 2 15\nP 1 15\nP 1 15',           # duplicate
            'G 1 2 15\nP 1 15\nR\nP 1 15',       # extra after return
            'G 1 2 15\nG 1 2 15',                 # lost return
            'P 1 15\nP 1 15',                     # excess leading writes
            'G 1 130 15',                          # data/select bit wrong
            'G 1 0 15',                            # strobe not asserted
            'G 15 2 15',                           # unsupported register
        ):
            with self.subTest(events=events), self.assertRaises(ValueError):
                check(events)

if __name__ == '__main__':
    unittest.main()
