#!/usr/bin/env python3
"""Synthetic tests only; no ROM fixtures."""
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from card_back import normalize, extract

class CatalogTest(unittest.TestCase):
    def recipe(self):
        groups=[(2,10),(32,11),(33,21),(49,17),(42,4),(43,4),(39,3),(50,9)]
        sizes={2:2,32:3,33:3,49:3,42:2,43:4,50:1}
        result=[]
        for group,n in groups:
            for i in range(n):
                length=(28,18,14)[i] if group==39 else sizes[group]
                result.append(tuple([group<<10]+list(range(1,length))))
        return result
    def capture(self,mutation=False):
        commands=self.recipe();text=[]
        for sample in range(3):
            for i,words in enumerate(commands):
                w=list(words)
                if w[0]==0x8000:w[1]+=sample*16;w[2]+=sample*7
                if mutation and sample==2 and i==78:w[0]^=1
                text.append('V 1 1 1 '+' '.join('%04x'%v for v in w))
                if i==0:text.append('S '+' '.join(['0']*308))
        return '\n'.join(text)
    def test_translated_exact_recipe(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'catalog';p.write_text(self.capture())
            recipe,samples,_=extract(p)
            self.assertEqual(len(recipe),79)
            self.assertEqual(sum(map(len,recipe)),260)
            self.assertEqual(len(samples),3)
    def test_parameter_mutation_is_not_wildcard(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'catalog';p.write_text(self.capture(True))
            with self.assertRaisesRegex(ValueError,'one exact translated recipe'):extract(p)
    def test_signed_translation(self):
        recipe,anchor=normalize([(0x8000,0xfffc,4),(0x8000,3,10),(0x8400,0xfffe,3)])
        self.assertEqual(anchor,(-4,4))
        self.assertEqual(recipe,((0x8000,0,0),(0x8000,7,6),(0x8400,0xfffe,3)))

if __name__=='__main__':unittest.main()
