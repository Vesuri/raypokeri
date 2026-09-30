#!/usr/bin/env python3
import unittest
from release_graphics_probe import commands, prepare

BEGIN = 'GRAPHICS begin id=1 cycle=80000 frame=2 beam=10 count=2 words=0800,1111,\n'
END = 'GRAPHICS finish id=1 cycle=80000 frame=2 beam=20 group=2 done=1\n'


class ProbeTests(unittest.TestCase):
    def test_units_and_payload(self):
        r = commands(BEGIN + END)
        self.assertEqual(r[0]['words'], [0x0800, 0x1111])
        self.assertAlmostEqual(r[0]['seconds'], .000640)

    def test_incomplete_and_unpaired(self):
        for text in (BEGIN, END, BEGIN + BEGIN + END):
            with self.assertRaises(ValueError):
                commands(text)

    def test_invalid_completion(self):
        for end in (END.replace('group=2', 'group=3'), END.replace('done=1', 'done=0'),
                    END.replace('beam=20', 'beam=0')):
            with self.assertRaises(ValueError):
                commands(BEGIN + end)

    def test_truncation(self):
        with self.assertRaises(ValueError):
            commands(BEGIN.replace('count=2', 'count=65') + END)

    def test_template_scope(self):
        template = 'break *(&amigaInputKey)\nprintf "RELEASE key cycle=xxx'
        result = prepare(template)
        self.assertIn('nativeCycles + 8000000', result)
        self.assertIn('disable $graphics_begin_bp', result)
        with self.assertRaises(ValueError):
            prepare(template + template)
        with self.assertRaises(ValueError):
            prepare('no key probe')


if __name__ == '__main__':
    unittest.main()
