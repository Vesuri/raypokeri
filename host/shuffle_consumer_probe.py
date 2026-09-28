#!/usr/bin/env python3
"""Headless, ROM-dependent scheduling experiment; never changes a normal build.

Compile instrumented copies of the host entry point under tmp/. Compare the
existing producer wait against bounded consumer backpressure with display-state
retention. All ROM-derived traces/frames remain ignored. This is a prototype,
not a production timing policy or a native/physical-hardware validation.
"""
from pathlib import Path
import argparse
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = 'tmp/shuffle-consumer-experiment'
OBJECTS = ('m68kcpu m68kops m68kdasm softfloat board hd63484 hd63484drawing '
           'cardbackcache videooutput display serialpeer boardstate cpustate '
           'ayaudio wavoutput window').split()


def replace(source, old, new):
    if source.count(old) != 1:
        raise RuntimeError('host entry point changed; review prototype insertion: ' + old[:80])
    return source.replace(old, new)


def variants():
    original = (ROOT / 'host/main.cpp').read_text()
    # Preserve ordinary execution, watchdog and device models. The only changed
    # external behavior is FIFO readiness during the approved shuffle experiment.
    consumer = replace(original, 'static void stop(const char *why) {', r'''
struct Marker { uint32_t target;std::array<uint8_t,256> display; };
static std::deque<Marker> marks;
static uint64_t consumeUntil=0;
static unsigned consumed=0,marked=0;
static uint32_t ramLong(unsigned a){return (uint32_t(memory[a])<<24)|(uint32_t(memory[a+1])<<16)|(uint32_t(memory[a+2])<<8)|memory[a+3];}
static void beginHold(const Marker &marker){
    ++consumed;consumeUntil=(cycles/160000+1)*160000;
    auto live=board.video.control;board.video.control=marker.display;
    writeFrame("tmp/shuffle-consumer-experiment-consumer-step-"+std::to_string(consumed)+".ppm",compose(board.video));
    board.video.control=live;
    fprintf(stderr,"CONSUMED %u cycle=%llu queue=%zu\n",consumed,cycles,marks.size());
}
static unsigned gatedIrq(bool vector){
    auto saved=board.video.control[3];
    if(consumeUntil)board.video.control[3]&=~3;
    unsigned result=vector?board.vector():board.irq();board.video.control[3]=saved;return result;
}
static void stop(const char *why) {''')
    consumer = replace(consumer, '    return value;\n}\nstatic void writemem',
                       '    if(consumeUntil && address==0xf6000 && size==1)value&=~3u;\n'
                       '    return value;\n}\nstatic void writemem')
    start = consumer.index('    if(shuffleEnabled && pc==pokeri::ShuffleWait::pc){', consumer.index('static void hook('))
    end = consumer.index('    auto low=', start)
    consumer = consumer[:start] + r'''
    if(pc==0x1dfa0 || pc==0x1aa20)fprintf(stderr,"CALLBACK pc=%x cycle=%llu marked=%u consumed=%u\n",pc,cycles,marked,consumed);
    if(pc==pokeri::ShuffleWait::pc){
        uint32_t caller=relocation.canonical(readmem(m68k_get_reg(nullptr,M68K_REG_SP),4));
        if(!pokeri::ShuffleWait::caller(caller)){stop("prototype caller outside verified shuffle");return;}
        uint32_t target=ramLong(0x41326);++marked;
        fprintf(stderr,"MARK %u cycle=%llu producer=%x consumer=%x begin=%x end=%x\n",marked,cycles,target,ramLong(0x4132a),ramLong(0x413be),ramLong(0x413c2));
        if(marks.size()>=32){stop("prototype markers overflow");return;}
        marks.push_back({target,board.video.control});
        if(!consumeUntil && marks.size()==1 && target==ramLong(0x4132a)){
            auto marker=marks.front();marks.pop_front();beginHold(marker);
        }
    }
    if(pc==0x2e62 && !consumeUntil && !marks.empty() && m68k_get_reg(nullptr,M68K_REG_A1)==marks.front().target){
        auto marker=marks.front();marks.pop_front();beginHold(marker);
    }
''' + consumer[end:]
    consumer = replace(consumer, 'level==5?board.vector():24+level', 'level==5?gatedIrq(true):24+level')
    consumer = replace(consumer, 'if(devices) m68k_set_irq(board.irq());',
                       'if(consumeUntil && cycles>=consumeUntil)consumeUntil=0;\n'
                       '        if(devices) m68k_set_irq(gatedIrq(false));')
    consumer = replace(consumer, '    window.finishAudio();',
                       '    fprintf(stderr,"FINAL marked=%u consumed=%u queue=%zu\\n",marked,consumed,marks.size());\n'
                       '    window.finishAudio();')
    producer = replace(original,
                       '            shuffleTarget=(cycles/(board.config.cpuHz/50)+1)*(board.config.cpuHz/50);', r'''
        {
            writeFrame("tmp/shuffle-consumer-experiment-producer-step-"+std::to_string(shuffleSteps+1)+".ppm",compose(board.video));
            fprintf(stderr,"PRODUCER %llu cycle=%llu\n",shuffleSteps+1,cycles);
            shuffleTarget=(cycles/(board.config.cpuHz/50)+1)*(board.config.cpuHz/50);
        }''')
    return {'consumer': consumer, 'producer': producer}


def run(command, log):
    with (ROOT / log).open('w') as f:
        subprocess.run(command, cwd=ROOT, stdout=f, stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wrap', action='store_true', help='synthetically reposition an empty command ring to exercise wrap')
    args = parser.parse_args()
    (ROOT / 'tmp').mkdir(exist_ok=True)
    run(['make', 'harness'], BASE + '-build.log')
    ready = BASE + '-ready'
    run(['build/pokeri-host', '--devices', '--skip-hardware-tests', '--serial-peer',
         '--system-hz', '100', '--input-hz', '50', '--watchdog-ms', '400',
         '--watchdog-reset-us', '50000', '--ay-clock', '1000000', '--auto-setup',
         '--ms', '10000', '--out', ready, '--save-state', ready + '.state'], ready + '.log')
    (ROOT / (BASE + '.inputs')).write_text('11000 packet 3 0\n12000 1 0 0xfe\n12200 1 0 0xff\n')
    for kind, source in variants().items():
        if args.wrap:
            source = replace(source, '    Window window;if(windowRequested)window.open(cycles);', r'''
    auto get=[&](unsigned a){return (uint32_t(memory[a])<<24)|(uint32_t(memory[a+1])<<16)|(uint32_t(memory[a+2])<<8)|memory[a+3];};
    if(get(0x41326)!=get(0x4132a))throw std::runtime_error("wrap fixture requires an empty ring");
    uint32_t start=get(0x413be)+96;
    for(unsigned a:{0x41326u,0x4132au})for(unsigned i=0;i<4;++i)memory[a+i]=uint8_t(start>>(24-8*i));
    Window window;if(windowRequested)window.open(cycles);''')
        stem = BASE + '-' + kind
        (ROOT / (stem + '.cpp')).write_text(source)
        run(['clang++', '-std=c++11', '-O2', '-g', '-Ihost', '-Ihost/musashi', '-Ibuild',
             '-c', stem + '.cpp', '-o', stem + '.o'], stem + '-build.log')
        run(['clang++', stem + '.o'] + ['build/' + obj + '.o' for obj in OBJECTS] +
            ['-o', stem], stem + '-link.log')
        run([stem, '--devices', '--skip-hardware-tests', '--load-state', ready + '.state',
             '--inputs', BASE + '.inputs', '--shuffle-vblank' if kind == 'producer' else '--no-shuffle-vblank',
             '--stall-instructions', '0', '--ms', '13200', '--video-catalog', stem + '.catalog',
             '--out', stem], stem + '.log')
    logs = {kind: (ROOT / (BASE + '-' + kind + '.log')).read_text() for kind in ('producer', 'consumer')}
    steps = [tuple(map(int, m)) for m in re.findall(r'CONSUMED (\d+) cycle=(\d+) queue=(\d+)', logs['consumer'])]
    assert [n for n, _, _ in steps] == list(range(1, 31)), 'lost or duplicated shuffle step'
    assert 'FINAL marked=30 consumed=30 queue=0' in logs['consumer']
    for n, _, _ in steps:
        assert (ROOT / (BASE + '-consumer-step-' + str(n) + '.ppm')).read_bytes() == (
            ROOT / (BASE + '-producer-step-' + str(n) + '.ppm')).read_bytes(), f'frame {n} differs'
    commands = {}
    for kind in logs:
        commands[kind] = [(int(x.split()[1]), x.split()[4:]) for x in
                          (ROOT / (BASE + '-' + kind + '.catalog')).read_text().splitlines() if x.startswith('V ')]
    last_producer = int(re.findall(r'PRODUCER 30 cycle=(\d+)', logs['producer'])[0])
    a = [w for cycle, w in commands['producer'] if cycle <= last_producer]
    b = [w for cycle, w in commands['consumer'] if cycle <= steps[-1][1]]
    assert a == b, 'original command stream changed through the final shuffle frame'
    events = (ROOT / (BASE + '-consumer-events.txt')).read_text()
    assert 'watchdog CPU reset' not in events, 'watchdog reset'
    writes = [int(c) for c in re.findall(r'^AY .* cycle=(\d+)$', events, re.M)]
    during = sum(steps[0][1] <= c <= steps[-1][1] for c in writes)
    assert during > 0, 'sound sequence still held until shuffle completion'
    rows = [tuple(int(x, 16) for x in m) for m in re.findall(
        r'producer=(\w+) consumer=(\w+) begin=(\w+) end=(\w+)', logs['consumer'])]
    occupancy = max((p-c) % (end-begin) for p, c, begin, end in rows)
    wrapped = any(y[0] < x[0] for x, y in zip(rows, rows[1:]))
    if args.wrap:
        assert wrapped, 'fixture did not exercise a shuffle ring wrap'
    print(f'PASS: 30 byte-identical frames, unchanged {len(a)} commands, {during} AY writes during shuffle, no watchdog reset')
    print(f'Max observed producer-boundary occupancy: {occupancy}/{rows[0][3]-rows[0][2]} bytes; wrap={wrapped}')
    print('Prototype only: native integration, presentation retirement, snapshot and failure-state tests remain open.')


if __name__ == '__main__':
    main()
