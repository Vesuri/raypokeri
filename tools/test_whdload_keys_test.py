#!/usr/bin/env python3
import unittest
import zlib
from test_whdload_keys import validate


def image(value):
    body = b'PKAC0001' + bytes([value]) * 928
    return body + zlib.crc32(body).to_bytes(4, 'big')


def saves():
    return dict(zip(('nvram.bin', 'nvram.bak', 'accounting.bin', 'accounting.bak'),
                    (bytes(32768), bytes(32768), image(0), image(0))))


class PersistenceTests(unittest.TestCase):
    def test_emergency_unchanged(self):
        before = saves()
        validate('help', before, dict(before))
        after = dict(before, **{'accounting.bin': image(1)})
        with self.assertRaises(ValueError):
            validate('help', before, after)

    def test_escape_saves_and_backs_up(self):
        before = saves()
        after = dict(before, **{'accounting.bin': image(1)})
        validate('esc', before, after)
        for name, value in [('accounting.bak', image(2)), ('nvram.bin', b''),
                            ('accounting.bin', image(1)[:-1] + bytes([image(1)[-1] ^ 1]))]:
            broken = dict(after)
            broken[name] = value
            with self.assertRaises(ValueError):
                validate('esc', before, broken)

    def test_escape_requires_changed_progress(self):
        before = saves()
        with self.assertRaises(ValueError):
            validate('esc', before, dict(before))


if __name__ == '__main__':
    unittest.main()
