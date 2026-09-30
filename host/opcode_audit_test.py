#!/usr/bin/env python3
import tempfile
import unittest
from pathlib import Path
from opcode_audit import summarize

HEADER = 'pc,opcode,entries,valid68000,needs060isp\n'
class AuditTest(unittest.TestCase):
    def run_rows(self, text):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'audit.csv'
            path.write_text(HEADER + text)
            return summarize([path])

    def test_counts_changed_ram_and_unknown(self):
        result = self.run_rows('40100,0108,2,1,1\n40100,4e71,3,1,0\n00100,4afc,1,0,0\n')
        self.assertEqual(result['instruction_entries'], 6)
        self.assertEqual(result['changed_opcode_pcs'], ['40100'])
        self.assertEqual(result['ram_pcs'], 1)
        self.assertEqual(result['movep'], [{'pc': '40100', 'entries': 2}])
        self.assertEqual(result['unknown_68000_entries'], [{'pc': '00100', 'entries': 1}])

    def test_reject_invalid_or_contradictory_data(self):
        for rows in ['', '100,0108,0,1,1\n', '101,0108,2,1,1\n',
                     '100,0108,2,1,0\n', '100,4e71,2,2,0\n',
                     '100,0108,2,1,1\n100,0108,2,1,1\n']:
            with self.subTest(rows=rows), self.assertRaises(ValueError):
                self.run_rows(rows)

if __name__ == '__main__':
    unittest.main()
