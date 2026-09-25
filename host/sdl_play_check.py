#!/usr/bin/env python3
"""ROM-dependent standalone SDL smoke/regression check; all output is ignored."""
import os
from pathlib import Path
import select
import signal
import shutil
import wave
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
SDL = ROOT / 'build/pokeri-host-sdl'
HOST = ROOT / 'build/pokeri-host'
ENV = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
WORK = ROOT / 'tmp/sdl-play-check'
WORK.mkdir(exist_ok=True)


def run(args, cwd=ROOT):
    result = subprocess.run(args, cwd=cwd, env=ENV, capture_output=True, text=True, timeout=120)
    if result.returncode:
        raise AssertionError(result.stderr[-4000:] + result.stdout[-4000:])
    return result


# No options, arbitrary working directory, no captures, no implicit endpoint.
# Stop via the same SIGINT path as a terminal user, after reaching ready state.
import tempfile
cwd = Path(tempfile.mkdtemp(prefix='no-options-', dir=WORK))
before = set(cwd.rglob('*'))
p = subprocess.Popen([str(SDL)], cwd=cwd, env=ENV, stdout=subprocess.PIPE,
                     stderr=subprocess.PIPE, text=True, bufsize=1)
try:
    deadline = time.monotonic() + 120
    while True:
        if time.monotonic() > deadline:
            raise AssertionError('standalone startup timed out')
        if select.select([p.stdout], [], [], 1)[0]:
            line = p.stdout.readline()
            if line.startswith('Ready.'):
                break
            if not line and p.poll() is not None:
                raise AssertionError('standalone exited before ready: ' + p.stderr.read())
    time.sleep(1)
    assert p.poll() is None, 'unlimited play exited on its own'
    p.send_signal(signal.SIGINT)
    stdout, stderr = p.communicate(timeout=5)
    assert p.returncode == 0 and not stderr, (p.returncode, stderr)
    assert not [f for f in set(cwd.rglob('*')) - before if f.is_file()], 'unsolicited captures'
finally:
    if p.poll() is None:
        p.kill()
        p.wait()

# A zero play budget still initializes the cabinet, then stops without playing.
state = 'tmp/sdl-play-check/ready.state'
run([str(SDL), '--cold-boot', '--ms', '0', '--save-state', state])
warm = 'tmp/sdl-play-check/warm.state'
started = time.monotonic()
run([str(SDL), '--ms', '0', '--save-state', warm])
print(f'Cached startup: {time.monotonic()-started:.3f}s')
assert (ROOT / state).read_bytes() == (ROOT / warm).read_bytes(), 'cached full state differs from cold startup'
# Export only after initialization: avoid enormous boot traces in this check.
for name, path in [('actual', state), ('reference', warm)]:
    run([str(HOST), '--devices', '--load-state', path, '--ms', '0',
         '--palette-rom', '0', '--out', 'tmp/sdl-play-check/' + name])
for suffix in ['-ram.bin', '-vram.bin', '-indices.bin']:
    assert (WORK / ('actual' + suffix)).read_bytes() == (WORK / ('reference' + suffix)).read_bytes(), suffix

ram = (WORK / 'actual-ram.bin').read_bytes()
def word32(address):
    return int.from_bytes(ram[address-0x40000:address-0x40000+4], 'big')
assert word32(0x44074) == 0, 'startup gave the player credits'
assert word32(0x4400c) == 100, 'operator reserve was not initialized'
# A real coin must credit the empty machine through its original input handler.
coin = WORK / 'coin.inputs'
coin.write_text('40600 packet 3 0\n')
run([str(HOST), '--devices', '--load-state', state, '--inputs', str(coin),
     '--ms', '42000', '--out', 'tmp/sdl-play-check/coin'])
coin_ram = (WORK / 'coin-ram.bin').read_bytes()
assert int.from_bytes(coin_ram[0x4074:0x4078], 'big') == 1, 'coin did not add one credit'

# Relative budgets on restore and opt-in WAV/final-frame output.
shutil.copyfile(ROOT / state, cwd / 'tmp/ready.state')
run([str(SDL), '--load-state', 'tmp/ready.state', '--ms', '60', '--wav', '--frames'], cwd)
with wave.open(str(cwd / 'tmp/pokeri.wav')) as audio:
    assert 2646 <= audio.getnframes() <= 2647, 'play budget was not relative to restore'
assert (cwd / 'tmp/pokeri-final.ppm').is_file()
run([str(SDL), '--load-state', state, '--mute', '--instructions', '1'])
print('PASS: no-option playable startup, ROM-relative launch, unlimited run/clean quit, no default captures, paired ready RAM/VRAM/pixels, relative budget and opt-in captures')
