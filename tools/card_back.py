#!/usr/bin/env python3
"""Extract a local card-back recipe from fresh original-ROM execution.

The generated descriptor contains original graphics commands: never commit it.
--catalog is a research shortcut; the build/default always makes a fresh capture.
"""
import argparse
from collections import Counter
from pathlib import Path
import subprocess
import sys
import roms

ROOT = Path(__file__).resolve().parent.parent
SHAPE = Counter({2:10, 32:11, 33:21, 49:17, 42:4, 43:4, 39:3, 50:9})


def signed(n):
    return n - 65536 if n & 32768 else n


def normalize(commands):
    anchor = next((tuple(map(signed, w[1:])) for w in commands if w[0] == 0x8000), None)
    if anchor is None:
        raise ValueError('recipe has no absolute anchor')
    normalized = []
    for words in commands:
        w = list(words)
        if w[0] == 0x8000:
            w[1:] = [(signed(v)-a) & 65535 for v, a in zip(w[1:], anchor)]
        normalized.append(tuple(w))
    return tuple(normalized), anchor


def extract(path):
    commands, states, backgrounds = [], {}, {}
    for line in Path(path).read_text().splitlines():
        fields = line.split()
        if fields[0] == 'V':
            if fields[3] != '1':
                raise ValueError('catalog contains a failed command')
            commands.append(tuple(int(v,16) for v in fields[4:]))
        elif fields[0] == 'P':
            backgrounds[len(commands)-6] = fields[1]
        elif fields[0] == 'S':
            states[len(commands)-1] = tuple(int(v,16) for v in fields[1:])
    groups = {}
    for i in states:
        batch = commands[i:i+79]
        if len(batch) != 79 or Counter(w[0]>>10 for w in batch) != SHAPE:
            continue
        if sum(map(len,batch)) != 260:
            continue
        recipe, anchor = normalize(batch)
        groups.setdefault(recipe, []).append((i, anchor, states[i]))
    if len(groups) != 1:
        raise ValueError(f'expected one exact translated recipe, found {len(groups)}')
    recipe, samples = next(iter(groups.items()))
    if len(set(a for _, a, _ in samples)) < 3:
        raise ValueError('need at least three distinct translated samples')
    return recipe, samples, backgrounds


def emit(recipe, samples, output):
    words = [v for w in recipe for v in w]
    offsets, n = [], 0
    for w in recipe:
        offsets.append(n)
        n += len(w)
    offsets.append(n)
    # S is the context after the first WPR0. Later preparation replays that WPR.
    state = samples[0][2]
    if len(state) != 4+32+16+256:
        raise ValueError('incomplete drawing context')
    def array(name, values, kind='uint16_t'):
        return 'static const '+kind+' '+name+'[] = {\n'+''.join(
            '    '+','.join('0x%x'%v for v in values[i:i+12])+',\n'
            for i in range(0,len(values),12))+'};\n'
    text = '// GENERATED from verified local ROMs; do not commit. Schema 1.\n#pragma once\n#include <cstdint>\nnamespace pokeri { namespace card_recipe {\n'
    text += f'// {len(samples)} exact samples, {len(set(a for _,a,_ in samples))} distinct anchors.\n'
    text += array('words',words)+array('offsets',offsets)
    text += array('context',state,'uint32_t')
    text += '} }\n'
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(text)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--catalog',type=Path)
    p.add_argument('--output',type=Path,default=ROOT/'amiga/generated/CardBackRecipe.h')
    args = p.parse_args()
    if not roms.verify(roms.read_source(ROOT/'rom')):
        sys.exit('card-back: ROM identities differ')
    catalog = args.catalog
    if catalog is None:
        subprocess.run(['make','harness'],cwd=ROOT,check=True)
        work = ROOT/'tmp/card-back-catalog'
        work.mkdir(parents=True,exist_ok=True)
        for old in work.glob('fresh-nvram*'):
            old.unlink()
        catalog = work/'commands.txt'
        command = ['build/pokeri-host','--devices','--serial-peer','--system-hz','100',
                   '--input-hz','50','--watchdog-ms','400','--watchdog-reset-us','50000',
                   '--ay-clock','1000000','--palette-rom','0','--stall-instructions','100000000',
                   '--skip-hardware-tests','--ms','47500','--inputs','host/scenarios/play.inputs',
                   '--out','tmp/card-back-catalog/fresh','--video-catalog',str(catalog.relative_to(ROOT))]
        with (work/'run.log').open('w') as log:
            subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
    recipe, samples, backgrounds = extract(catalog)
    emit(recipe,samples,args.output)
    if backgrounds:
        sample_path = args.output.with_name('CardBackSamples.h')
        text = '// GENERATED local-ROM backgrounds; do not commit.\n#pragma once\n'
        text += 'static const char *const cardBackgrounds[] = {\n'
        for i, _, _ in samples:
            bg = backgrounds[i]
            if len(bg) != 8800:
                raise ValueError('incomplete background sample')
            text += '"'+bg+'",\n'
        text += '};\n'
        sample_path.write_text(text)
    print(f'card-back: {len(samples)} exact samples at {len(set(a for _,a,_ in samples))} anchors; 79 commands / 260 words -> {args.output}')

if __name__ == '__main__':
    main()
