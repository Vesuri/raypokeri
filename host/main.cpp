// Host-only execution and diagnostics. Original ROM instructions are never patched.
#include "m68k.h"
#include "VideoOutput.h"
#include "WavOutput.h"
#include "Window.h"
#include "AccessGate.h"
#include "Relocation.h"
#include "RomIdentity.h"
#include "../src/board/Board.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cmath>
#include <limits>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <sys/stat.h>
struct InputEvent { uint64_t cycle; unsigned pia,side,value; };
static std::vector<InputEvent> inputEvents;
extern "C" unsigned pokeri_cpu_state_size();
extern "C" void pokeri_cpu_state(void*,int);
static pokeri::Board board;
static AccessGate accessGate;
static Relocation relocation;
static int borrowedAddressRegister=-1;
struct RamWriter {uint32_t pc=0,address=0,size=0,value=0;uint64_t instruction=0;};
static std::map<uint32_t,RamWriter> ramWriters;
static RamWriter pendingMovemHalf;
static bool movemHalfPending=false;
static std::set<std::tuple<unsigned,unsigned,unsigned,char>> lowAccesses,romWrites;
static auto &memory = board.memory;
static bool devices;
static uint64_t irqCount;
static std::array<uint8_t, 0x20000> coverage{};
static std::array<uint32_t, 128> history{};
static uint64_t instructions, cycles, lastNew, stallLimit=20000000;
static uint32_t pc;
static unsigned breakpoint=0xffffffff, watchWrite=0xffffffff;
static bool stopped, probe;
static std::string reason;
static FILE *trace, *events;
static std::array<bool, 0x100000> reported{};
static void context(FILE *f);
static void stop(const char *why);
extern "C" void pokeri_exception(unsigned vector) {
    fprintf(events,"exception vector=%u pc=%05x instruction=%llu\n",vector,pc,instructions);
    context(events);
    if(vector<25 || vector==31) stop("CPU exception (vector recorded in events)");
}

static void stop(const char *why) { if (!stopped) reason=why; stopped=true; m68k_end_timeslice(); }
static uint32_t readmem(uint32_t address, unsigned size) {
    if(relocation.enabled && address>=relocation.rom+0x40000 && address-relocation.rom-0x40000<32){
        auto i=relocation.lowHooks.find(pc);unsigned offset=address-relocation.rom-0x40000;
        if(borrowedAddressRegister<0 || i==relocation.lowHooks.end() || i->second.offset!=offset || i->second.size!=size){stop("unguarded low-vector shadow access");return 0;}
        uint32_t value=0;for(unsigned b=0;b<size;++b)value=(value<<8)|relocation.vectorShadow[offset+b];return value;
    }
    uint32_t actual=address;
    if(!relocation.resetVectors && address<0x20)lowAccesses.emplace(pc,address,size,'R');
    if(relocation.resetVectors && address<8)address+=relocation.rom;
    uint32_t mappedStart=address;
    address=relocation.canonical(address);
    if(relocation.enabled && address!=0xffffffff && relocation.canonical(mappedStart+size-1)!=address+size-1){
        fprintf(events,"relocation boundary miss pc=%05x address=%06x size=%u\n",pc,actual,size);context(events);stop("access crosses relocated range boundary");return 0;
    }
    if(address==0xffffffff){fprintf(events,"relocation miss pc=%05x address=%06x size=%u\n",pc,actual,size);context(events);stop("unmapped relocated address");return 0;}
    if(address+size>memory.size() && !accessGate.permits(pc,address,size,'R')) {
        fprintf(events,"I/O table miss pc=%05x address=%05x size=%u direction=R\n",pc,address,size);
        context(events);stop("I/O access outside audited table");return 0;
    }
    uint32_t value=0;
    for (unsigned i=0;i<size;++i) {
        uint32_t a=(address+i)&0xfffff;
        value=(value<<8) | (devices ? board.read8(a) : (a<memory.size()? memory[a]:0));
    }
    if (address+size>memory.size()) {
        if ((!devices || board.fault) && !reported[address]) {fprintf(events,"unmapped read %05x size=%u\n",address,size);context(events);reported[address]=true;}
        fprintf(trace,"%llu,%05x,%05x,%u,R,%08x,%s,%06x\n",instructions,pc,address,size,value,devices?board.name(address):"unmapped",actual);
        if (devices ? board.fault : !probe) stop(devices?board.faultReason:"unmapped read");
    }
    return value;
}
static void writemem(uint32_t address,unsigned size,uint32_t value) {
    uint32_t actual=address;
    if(address<0x20)lowAccesses.emplace(pc,address,size,'W');
    address=relocation.canonical(address);
    if(relocation.enabled && address!=0xffffffff && relocation.canonical(actual+size-1)!=address+size-1){
        fprintf(events,"relocation boundary write miss pc=%05x address=%06x size=%u\n",pc,actual,size);context(events);stop("access crosses relocated range boundary");return;
    }
    if(address==0xffffffff){fprintf(events,"relocation write miss pc=%05x address=%06x size=%u\n",pc,actual,size);context(events);stop("unmapped relocated address");return;}
    if(address+size>memory.size() && !accessGate.permits(pc,address,size,'W')) {
        fprintf(events,"I/O table miss pc=%05x address=%05x size=%u direction=W\n",pc,address,size);
        context(events);stop("I/O access outside audited table");return;
    }
    if(!ramWriters.empty() && address>=0x40000 && address+size<=memory.size()){
        uint32_t start=address,width=size,whole=value;
        // Musashi emits MOVEM.L predecrement as low-word then high-word bus
        // writes. Record the logical saved register, including either surviving
        // half, without changing those writes or emulating the instruction.
        unsigned opcode=pc+1<memory.size()?(unsigned(memory[pc])<<8)|memory[pc+1]:0;
        if(size==2 && (opcode&0xfff8)==0x48e0){
            if(movemHalfPending && pendingMovemHalf.pc==pc && pendingMovemHalf.instruction==instructions && pendingMovemHalf.address==address+2){
                width=4;whole=(value<<16)|(pendingMovemHalf.value&0xffff);movemHalfPending=false;
            } else {pendingMovemHalf.pc=pc;pendingMovemHalf.address=address;pendingMovemHalf.value=value;pendingMovemHalf.instruction=instructions;movemHalfPending=true;}
        } else movemHalfPending=false;
        for(auto i=ramWriters.lower_bound(start);i!=ramWriters.end() && i->first<start+width;++i){
            i->second.pc=pc;i->second.address=start;i->second.size=width;i->second.value=whole;i->second.instruction=instructions;
        }
    }
    if(address<0x40000)romWrites.emplace(pc,address,size,'W');
    for (unsigned i=0;i<size;++i) {
        uint32_t a=(address+i)&0xfffff;
        if(a==watchWrite) {fprintf(events,"watched write %05x=%02x pc=%05x\n",a,(value>>(8*(size-1-i)))&255,pc);context(events);}
        if(devices) board.write8(a,value>>(8*(size-1-i)));
        else if(a>=0x40000 && a<memory.size()) memory[a]=value>>(8*(size-1-i));
    }
    if(address+size>memory.size()) {
        if ((!devices || board.fault) && !reported[address]) {fprintf(events,"unmapped write %05x size=%u\n",address,size);context(events);reported[address]=true;}
        fprintf(trace,"%llu,%05x,%05x,%u,W,%08x,%s,%06x\n",instructions,pc,address,size,value,devices?board.name(address):"unmapped",actual);
        if(devices ? board.fault : !probe) stop(devices?board.faultReason:"unmapped write");
    }
}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return readmem(a,1);}
unsigned m68k_read_memory_16(unsigned a){return readmem(a,2);}
unsigned m68k_read_memory_32(unsigned a){return readmem(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){writemem(a,1,v);}
void m68k_write_memory_16(unsigned a,unsigned v){writemem(a,2,v);}
void m68k_write_memory_32(unsigned a,unsigned v){writemem(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return a<memory.size()?memory[a]:0;}
unsigned m68k_read_disassembler_16(unsigned a){return (m68k_read_disassembler_8(a)<<8)|m68k_read_disassembler_8(a+1);}
unsigned m68k_read_disassembler_32(unsigned a){return (m68k_read_disassembler_16(a)<<16)|m68k_read_disassembler_16(a+2);}
}
static void hook(unsigned address) {
    pc=relocation.canonical(address);
    if(pc>=0x80000){stop("instruction outside relocated ROM/RAM");return;}
    auto low=relocation.lowHooks.find(pc);
    if(relocation.enabled && low!=relocation.lowHooks.end() && m68k_get_reg(nullptr,m68k_register_t(M68K_REG_A0+low->second.reg))==0){
        borrowedAddressRegister=low->second.reg;
        m68k_set_reg(m68k_register_t(M68K_REG_A0+borrowedAddressRegister),relocation.rom+0x40000);
    }
    auto control=relocation.controls.find(pc);
    if(relocation.enabled && control!=relocation.controls.end() && control->second.kind=="set_ram_delta_d7")
        m68k_set_reg(M68K_REG_D7,relocation.ram-0x40000);
    history[instructions++%history.size()]=pc;
    if(!(coverage[pc>>3]&(1<<(pc&7)))) lastNew=instructions;
    coverage[pc>>3]|=1<<(pc&7);
    if(pc==breakpoint) {context(events);stop("breakpoint");}
    if(devices && pc==0x24fa) {context(events);stop("ROM fatal startup error (D0 in context)");}
    if(instructions-lastNew>stallLimit) stop("stall: no new PC within instruction window");
}
static FILE *openfile(const std::string &name,const char *mode) {
    FILE *f=fopen(name.c_str(),mode); if(!f) throw std::runtime_error("cannot open "+name); return f;
}
static void context(FILE *f) {
    fprintf(f,"instructions=%llu cycles=%llu pc=%05x sr=%04x\n",instructions,cycles,m68k_get_reg(nullptr,M68K_REG_PC),m68k_get_reg(nullptr,M68K_REG_SR));
    for(unsigned r=0;r<16;++r) fprintf(f,"%c%u=%08x%c",r<8?'D':'A',r%8,m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r)),r%4==3?'\n':' ');
    for(uint64_t i=instructions>history.size()?instructions-history.size():0;i<instructions;++i) {
        unsigned p=history[i%history.size()];char asmtext[256];
        m68k_disassemble(asmtext,p,M68K_CPU_TYPE_68000);fprintf(f,"%05x %s\n",p,asmtext);
    }
}
static uint64_t number(const char *s) {
    size_t end=0;
    if(!*s || *s=='-') throw std::runtime_error("expected nonnegative integer");
    uint64_t n=std::stoull(s,&end,0);
    if(s[end]) throw std::runtime_error("invalid integer");
    return n;
}
static uint32_t placement(const char *s){uint64_t v=number(s);if(v>0xffffff)throw std::runtime_error("placement exceeds 24 bits");return uint32_t(v);}
static void require(bool ok,const char *message) {if(!ok) throw std::runtime_error(message);}
static void selftest() {
    m68k_init();m68k_set_cpu_type(M68K_CPU_TYPE_68000);m68k_set_instr_hook_callback(hook);
    memory[0]=0x12;memory[1]=0x34;
    writemem(0,2,0);require(readmem(0,2)==0x1234,"ROM writes must be ignored");
    writemem(0x40000,4,0x12345678);require(readmem(0x140000,4)==0x12345678,"RAM endian/mask");
    probe=true;require(readmem(0xfffff,2)==0x12,"20-bit wrapping read");
    probe=false;readmem(0xc0000,1);require(stopped,"unknown read must stop");stopped=false;
    writemem(0xe0000,1,0);require(stopped,"unknown write must stop");stopped=false;
    memory.fill(0);memory[1]=4;memory[2]=0x10;memory[6]=1; // SSP=41000 PC=100
    memory[0x100]=0x4e;memory[0x101]=0x71; // original synthetic NOP
    memory[0x102]=0x46;memory[0x103]=0xfc;memory[0x104]=0x27; // MOVE #2700,SR
    memory[0x21]=0;memory[0x22]=2; // privilege vector -> 200
    m68k_pulse_reset();
    while(instructions==0) m68k_execute(1);
    require(instructions==1 && m68k_get_reg(nullptr,M68K_REG_PC)==0x102,"one-instruction execution");
    m68k_set_reg(M68K_REG_SR,0);m68k_execute(1);
    require(stopped && m68k_get_reg(nullptr,M68K_REG_PC)==0x200,"privilege exception capture");
    require(coverage[0x100>>3]&1,"coverage bitmap");
    puts("PASS: ROM write protection, RAM endianness, 20-bit mask/wrap, unknown-access stops, instruction count, privilege exception, coverage");
}
static int acknowledge(int level) {++irqCount;return level==5?board.vector():M68K_INT_ACK_AUTOVECTOR;}
static void deviceLog(const char *name,unsigned reg,uint8_t value) {fprintf(events,"%s register=%u value=%02x pc=%05x instruction=%llu\n",name,reg,value,pc,instructions);}
static uint64_t videoLogged;
// Every HD63484 command, up to a cap; the full counts go to <out>-devices.txt.
static void videoCommand(const uint16_t *w,unsigned n,bool executed) {
    if(++videoLogged>20000 && !board.video.error) return;
    fprintf(events,"HD63484 %-5s%s pc=%05x instruction=%llu words=",pokeri::Hd63484::mnemonic(w[0]),executed?"":" (not executed)",pc,instructions);
    for(unsigned i=0;i<n && i<12;++i) fprintf(events,"%s%04x",i?" ":"",w[i]);
    fprintf(events,"%s\n",n>12?" ...":"");
}
static void cpuReset() {
    relocation.resetVectors=true;m68k_pulse_reset();relocation.resetVectors=false;
    // Host virtual exception vector base; Phase 4 supplies synthetic exceptions.
    if(relocation.enabled)m68k_set_reg(M68K_REG_VBR,relocation.rom);
}
static void resetInstruction() {
    if(relocation.enabled && !relocation.resetHooks.count(pc)){context(events);stop("RESET outside hook table");return;}
    if(devices)board.reset();
}
int main(int argc,char **argv) try {
    uint64_t limit=10000000, cycleLimit=UINT64_MAX,budgetMs=UINT64_MAX; double hz=8000000;
    std::string out="tmp/phase0", rom="rom", inputPath,saveState,loadState,retainedRam,codeMap,relocTable="host/tables/relocations.csv",lowHookTable="host/tables/low-vector-hooks.csv",controlTable="host/tables/control-hooks.csv",resetTable="host/tables/reset-hooks.csv",provenancePath; unsigned disasm=0, disasmEnd=0; bool test=false,audio=false,liveAudio=false,windowRequested=false;int paletteBank=-1; unsigned frameEvery=0,frameHz=50;uint64_t nextFrame=0,frameNumber=0;
    for(int i=1;i<argc;++i) {
        std::string a=argv[i];
        if(a=="--bypass-module-checksums") {relocation.bypass=true;continue;}
        if(a=="--window") {windowRequested=true;continue;}
        if(a=="--live-audio") {liveAudio=true;continue;}
        if(a=="--wav") {audio=true;continue;}
        if(a=="--serial-peer") {board.peer.enabled=true;continue;}
        if(a=="--devices") {devices=true;continue;}
        if(a=="--self-test") {test=true;continue;}
        if(a=="--probe") {probe=true;continue;}
        if(a=="--help") {puts("pokeri-host [--instructions N | --ms N] [--clock Hz] [--out tmp/name] [--rom-dir rom] [--probe] [--stall-instructions N] [--break-pc address]\n--devices enables partial portable models; --system-hz N, --input-hz N and --watchdog-ms N enable experimental external signals (default off).\n--video-kwords N: installed HD63484 memory in K words (power of two; default 256 = 512 KB, the target variant; 1024 = 2 MB).\n--probe: Phase 0 logging stubs return zero and continue until stall. Default stops at first unknown access.\n--watchdog-reset-us N: explicit reset delay after warning (research profile: 50000).\n--inputs PATH: absolute-time PIA/serial input script; --serial-peer enables the diagnostic transport peer.\n--frame-every N --frame-hz N: periodic PPM capture; default cadence hypothesis 50 Hz. Final frame always saved.\n--palette-rom 0..3: test the ROM RAMDAC palette at runtime; default is labelled placeholder.\n--ay-clock Hz --wav: explicit AY oscillator hypothesis and mono 44100 Hz WAV capture.\n--save-state tmp/file --load-state tmp/file: full instruction-boundary state, same ROM/core ABI.\n--retained-ram tmp/file: experimental full main-RAM retention across a fresh CPU boot.\n--window: SDL build only (make harness SDL=1, build/pokeri-host-sdl).\n--live-audio: play AY sound with --window; requires an AY clock (explicit or restored). May be combined with --wav.\n--bypass-module-checksums: explicit temporary bypass after verifying all four SHA-256 hashes.\n--rom-base N --ram-base N --device-base N: strict 24-bit relocated mode, old address ranges unmapped.\n--relocation-table CSV --low-vector-hooks CSV --control-hooks CSV --reset-hooks CSV: explicit patch/hook metadata.\n--ram-provenance PATH: preserve last-writer evidence for selected RAM bytes across checkpoints.\n--code-map COVERAGE: export covered ROM instruction lengths for research.\n--io-table CSV: reject hardware accesses outside the audited PC/address/size/direction table.\nBudgets are absolute emulated endpoints, including after restore. Clock defaults to UNMEASURED 8 MHz; Musashi uses 68000 cycle timing, not 68008 bus timing.");return 0;}
        if(i+1==argc) throw std::runtime_error("missing option value");
        const char *v=argv[++i];
        if(a=="--break-pc") breakpoint=number(v);
        else if(a=="--ay-clock") {uint64_t n=number(v);if(n<100000||n>10000000)throw std::runtime_error("AY clock outside research range");board.ay.clockHz=n;}
        else if(a=="--watch-write") watchWrite=number(v);
        else if(a=="--system-hz") board.config.systemHz=number(v);
        else if(a=="--input-hz") board.config.inputHz=number(v);
        else if(a=="--watchdog-reset-us") board.config.watchdogResetUs=number(v);
        else if(a=="--watchdog-ms") board.config.watchdogMs=number(v);
        else if(a=="--video-kwords") {uint64_t k=number(v);if(k<1||k>1024||(k&(k-1))) throw std::runtime_error("--video-kwords must be a power of two, 1-1024");board.video.frameMask=uint32_t(k*1024-1);}
        else if(a=="--rom-base") {relocation.enabled=true;relocation.rom=placement(v);}
        else if(a=="--ram-base") {relocation.enabled=true;relocation.ram=placement(v);}
        else if(a=="--device-base") {relocation.enabled=true;relocation.guard=placement(v);}
        else if(a=="--ram-provenance") provenancePath=v;
        else if(a=="--reset-hooks") resetTable=v;
        else if(a=="--control-hooks") controlTable=v;
        else if(a=="--low-vector-hooks") lowHookTable=v;
        else if(a=="--relocation-table") relocTable=v;
        else if(a=="--io-table") accessGate.load(v);
        else if(a=="--code-map") codeMap=v;
        else if(a=="--disasm") disasm=number(v);
        else if(a=="--disasm-end") disasmEnd=number(v);
        else if(a=="--stall-instructions") stallLimit=number(v);
        else if(a=="--palette-rom") {paletteBank=number(v);if(paletteBank<0||paletteBank>3)throw std::runtime_error("palette bank must be 0-3");}
        else if(a=="--frame-every") frameEvery=number(v);
        else if(a=="--frame-hz") frameHz=number(v);
        else if(a=="--instructions") limit=number(v);
        else if(a=="--clock") hz=std::stod(v);
        else if(a=="--ms") {budgetMs=cycleLimit=number(v);limit=UINT64_MAX;}
        else if(a=="--inputs") inputPath=v;
        else if(a=="--retained-ram") retainedRam=v;
        else if(a=="--save-state") saveState=v;
        else if(a=="--load-state") loadState=v;
        else if(a=="--out") out=v;
        else if(a=="--rom-dir") rom=v;
        else throw std::runtime_error("unknown option "+a);
    }
    if(!std::isfinite(hz) || hz<1 || hz>1000000000) throw std::runtime_error("clock must be between 1 and 1000000000 Hz");
    if(cycleLimit!=UINT64_MAX) {
        long double n=cycleLimit*(long double)hz/1000;
        if(n>=static_cast<long double>(UINT64_MAX)) throw std::runtime_error("time budget overflow");
        cycleLimit=uint64_t(n);
    }
    if(out.compare(0,4,"tmp/") || out.find("..")!=std::string::npos) throw std::runtime_error("output must be under tmp/");
    mkdir("tmp",0755);
    if(test) {trace=openfile("tmp/selftest-trace.csv","w");events=openfile("tmp/selftest-events.txt","w");selftest();fclose(trace);fclose(events);return 0;}
    // Address order, NOT name order: 30 at $00000, 38 at $10000, 34 at $20000 (docs/rom-set.md —
    // the ROM's own module checksum passes only in this order).
    const char *chips[]={"77POK30","77POK38","77POK34","PARA200J"};
    for(unsigned i=0;i<4;++i) {FILE*f=openfile(rom+"/"+chips[i],"rb");size_t n=fread(memory.data()+i*65536,1,65536,f);int extra=fgetc(f);fclose(f);if(n!=65536 || extra!=EOF) throw std::runtime_error("wrong ROM size");}
    if(accessGate.active() || relocation.enabled || relocation.bypass)verifyRomIdentity(memory.data());
    if(paletteBank>=0){std::array<unsigned,16> colors{};for(unsigned i=0;i<16;++i)for(unsigned c=0;c<3;++c)colors[i]=(colors[i]<<8)|(unsigned(memory[0x5d76+paletteBank*48+i*3+c])*255/63);setFramePalette(colors);}
    if(!codeMap.empty()) {
        std::array<uint8_t,0x20000> map{};
        FILE*f=openfile(codeMap,"rb");size_t n=fread(map.data(),1,map.size(),f);int extra=fgetc(f);fclose(f);
        require(n==map.size() && extra==EOF,"invalid coverage bitmap for code map");
        f=openfile(out+"-code.csv","w");fprintf(f,"pc,length\n");
        FILE*listing=openfile(out+"-code-asm.txt","w");
        for(unsigned a=0;a<0x40000;a+=2)if(map[a>>3]&(1<<(a&7))) {
            char line[256];unsigned length=m68k_disassemble(line,a,M68K_CPU_TYPE_68000);
            fprintf(f,"%06x,%u\n",a,length);fprintf(listing,"%06x %s\n",a,line);
        }
        require(fclose(f)==0 && fclose(listing)==0,"code map write failed");return 0;
    }
    if(disasmEnd>0x100000 || disasm>=0x100000 || (disasmEnd && disasmEnd<=disasm)) throw std::runtime_error("invalid disassembly range");
    if(disasmEnd) {FILE*f=openfile(out+"-disasm.txt","w");for(unsigned a=disasm;a<disasmEnd;) {char line[256];unsigned n=m68k_disassemble(line,a,M68K_CPU_TYPE_68000);fprintf(f,"%05x %s\n",a,line);a+=n;}fclose(f);return 0;}
    if(!provenancePath.empty()){
        std::ifstream f(provenancePath);uint32_t address;
        if(!f)throw std::runtime_error("cannot open RAM provenance watch list");
        while(f>>std::hex>>address){if(address<0x40000 || address>=0x80000 || !ramWriters.emplace(address,RamWriter{}).second)throw std::runtime_error("invalid/duplicate RAM provenance address");}
        if(!f.eof() || ramWriters.empty())throw std::runtime_error("invalid RAM provenance watch list");
    }
    relocation.validate();
    if(relocation.enabled && (!devices || !accessGate.active()))throw std::runtime_error("relocation requires --devices and --io-table");
    if(relocation.bypass || relocation.enabled)relocation.loadControls(controlTable);
    if(relocation.enabled){relocation.loadLowHooks(lowHookTable);relocation.loadResetHooks(resetTable);}
    relocation.patch(memory,relocTable);
    trace=openfile(out+"-trace.csv","w");fprintf(trace,"instruction,pc,address,size,direction,value,device,cpu_address\n");
    events=openfile(out+"-events.txt","w");
    board.config.cpuHz=hz;board.log=deviceLog;board.video.commandLog=videoCommand;
    if(board.config.systemHz>1000000 || board.config.inputHz>1000000) throw std::runtime_error("signal frequency too high");
    if(devices) puts("EXPERIMENTAL board model: external signal rates and CPU clock are hypotheses; boot success is not hardware validation.");
    if(devices) { FILE*f=fopen((out+"-nvram.bin").c_str(),"rb");if(f) {require(fread(board.nvram.bytes.data(),1,0x8000,f)==0x8000 && fgetc(f)==EOF,"invalid NVRAM image");fclose(f);} }
    if(!retainedRam.empty()) {
        if(retainedRam.compare(0,4,"tmp/") || retainedRam.find("..")!=std::string::npos)throw std::runtime_error("retained RAM must be under tmp/");
        if(!loadState.empty())throw std::runtime_error("choose retained RAM cold boot or full snapshot restore");
        FILE*f=fopen(retainedRam.c_str(),"rb");if(f){require(fread(memory.data()+0x40000,1,0x40000,f)==0x40000 && fgetc(f)==EOF,"invalid retained RAM image");fclose(f);}else if(errno!=ENOENT)throw std::runtime_error("cannot read retained RAM image");
    }
    m68k_init();m68k_set_cpu_type(M68K_CPU_TYPE_68000);m68k_set_instr_hook_callback(hook);m68k_set_int_ack_callback(acknowledge);m68k_set_reset_instr_callback(resetInstruction);cpuReset();
    if(!frameHz || frameHz>1000) throw std::runtime_error("invalid frame frequency");
    nextFrame=uint64_t(hz)/frameHz;
    size_t nextInput=0;
    uint64_t inactive=0;
    auto snapshot=[&](const std::string &path,bool reading){
        if(path.compare(0,4,"tmp/") || path.find("..")!=std::string::npos)throw std::runtime_error("state must be under tmp/");
        pokeri::State s;s.reading=reading;
        if(reading){std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("cannot read state");auto size=f.tellg();if(size<0 || size>16*1024*1024)throw std::runtime_error("invalid state file size");s.bytes.resize(size_t(size));f.seekg(0);if(!f.read(reinterpret_cast<char*>(s.bytes.data()),size))throw std::runtime_error("cannot read complete state");}
        uint64_t romHash=14695981039346656037ull;
        for(size_t i=0;i<0x40000;++i)romHash=(romHash^memory[i])*1099511628211ull;
        uint64_t hash=romHash,magic=0x3154534952454b50ull;
        uint32_t expectedVersion=provenancePath.empty()?(relocation.enabled?3:2):4;
        uint32_t version=expectedVersion,abi=pokeri_cpu_state_size(),endian=0x12345678;
        uint8_t nativeEndian=*reinterpret_cast<uint8_t*>(&endian),storedEndian=nativeEndian;
        s.fields(magic,version,hash,abi,storedEndian);
        if(magic!=0x3154534952454b50ull || version!=expectedVersion || hash!=romHash || abi!=pokeri_cpu_state_size() || storedEndian!=nativeEndian)throw std::runtime_error("state version/ROM/CPU ABI mismatch");
        if(version>=3){uint32_t rom=relocation.rom,ram=relocation.ram,guard=relocation.guard;s.fields(rom,ram,guard);
            if(rom!=relocation.rom || ram!=relocation.ram || guard!=relocation.guard)throw std::runtime_error("snapshot relocation placement mismatch");}
        std::vector<uint8_t> cpu(abi);if(!reading)pokeri_cpu_state(cpu.data(),0);s.value(cpu);
        if(cpu.size()!=abi)throw std::runtime_error("CPU state length mismatch");
        board.state(s);
        s.fields(coverage,history,reported,instructions,cycles,lastNew,pc,irqCount,videoLogged,inactive,nextFrame,frameNumber,frameHz);
        uint64_t inputIndex=nextInput;uint32_t inputCount=inputEvents.size();s.fields(inputIndex,inputCount);
        if(inputCount>1000000 || inputIndex>inputCount)throw std::runtime_error("invalid state input queue");
        if(reading)inputEvents.resize(inputCount);
        for(auto &e:inputEvents)s.fields(e.cycle,e.pia,e.side,e.value);
        if(version>=4){
            uint32_t count=ramWriters.size();s.value(count);if(count!=ramWriters.size())throw std::runtime_error("snapshot RAM provenance watch list mismatch");
            for(auto &entry:ramWriters){uint32_t address=entry.first;auto &w=entry.second;s.fields(address,w.pc,w.address,w.size,w.value,w.instruction);
                if(address!=entry.first)throw std::runtime_error("snapshot RAM provenance address mismatch");}
        }
        nextInput=inputIndex;
        if(reading){if(s.cursor!=s.bytes.size())throw std::runtime_error("trailing state data");pokeri_cpu_state(cpu.data(),1);hz=board.config.cpuHz;}
        else {FILE*f=openfile(path,"wb");size_t n=fwrite(s.bytes.data(),1,s.bytes.size(),f);int result=fclose(f);if(n!=s.bytes.size()||result)throw std::runtime_error("cannot write state");}
    };
    if(!loadState.empty()){if(!devices)throw std::runtime_error("state requires --devices");snapshot(loadState,true);if(budgetMs!=UINT64_MAX){long double n=budgetMs*(long double)hz/1000;if(n>=static_cast<long double>(UINT64_MAX))throw std::runtime_error("time budget overflow");cycleLimit=uint64_t(n);}}
    if(!frameHz || frameHz>1000 || frameHz>hz)throw std::runtime_error("invalid restored frame frequency");
    if(!inputPath.empty()) {
        inputEvents.clear();nextInput=0;
        std::ifstream in(inputPath); if(!in)throw std::runtime_error("cannot open input script");
        std::string line;
        while(std::getline(in,line)) {
            line=line.substr(0,line.find('#'));if(line.find_first_not_of(" \t\r")==std::string::npos)continue;
            std::istringstream row(line);std::string ms,pia,side,value,extra;
            if(!(row>>ms>>pia>>side>>value) || (row>>extra))throw std::runtime_error("input row: milliseconds pia side value");
            long double when=number(ms.c_str())*(long double)hz/1000;if(when>=static_cast<long double>(UINT64_MAX))throw std::runtime_error("input time overflow");
            if(number(side.c_str())>UINT32_MAX || number(value.c_str())>UINT32_MAX)throw std::runtime_error("input value overflow");
            InputEvent e{uint64_t(when),pia=="packet"?4u:pia=="rx"?3u:unsigned(number(pia.c_str())),unsigned(number(side.c_str())),unsigned(number(value.c_str()))};
            if(e.pia>4 || (e.pia==4?(e.side>63 || e.value>0x2ffff):(e.side>(e.pia==3?0u:1u) || e.value>255)) || (!inputEvents.empty() && e.cycle<inputEvents.back().cycle))throw std::runtime_error("invalid input range/order");
            if(e.pia==4 && !board.peer.enabled)throw std::runtime_error("packet input requires --serial-peer");
            if(inputEvents.size()>=1000000)throw std::runtime_error("too many input events");
            inputEvents.push_back(e);
        }
    }
    if(liveAudio && !windowRequested)throw std::runtime_error("--live-audio requires --window");
    if((audio || liveAudio) && !board.ay.clockHz)throw std::runtime_error("audio requires --ay-clock or a snapshot with an AY clock");
    Window window;if(windowRequested)window.open(cycles);
    if(liveAudio)window.openAudio();
    WavOutput wav;if(audio)wav.open(out+".wav");
    struct AudioOutput : pokeri::Tone {
        WavOutput *wav=nullptr;Window *window=nullptr;
        void sample(int16_t value) override {if(wav)wav->sample(value);if(window)window->sample(value);}
    } output;
    output.wav=audio?&wav:nullptr;output.window=liveAudio?&window:nullptr;
    if(audio || liveAudio)board.ay.sink=&output;
    while(nextInput<inputEvents.size() && inputEvents[nextInput].cycle<cycles)++nextInput;
    while(!stopped && instructions<limit && cycles<cycleLimit) {
        while(nextInput<inputEvents.size() && inputEvents[nextInput].cycle<=cycles) {
            auto e=inputEvents[nextInput++];if(e.pia==4){std::vector<uint8_t> p{uint8_t(e.side)};unsigned n=e.value>>16;if(n>2)throw std::runtime_error("packet payload length");if(n==2)p.push_back(e.value>>8);if(n)p.push_back(e.value);board.peer.enqueue(p);}else if(e.pia==3)board.serial[e.side].receive.push_back(e.value);else board.pia[e.pia].input[e.side]=e.value;
            fprintf(events,"input cycle=%llu kind=%s device=%u register=%u value=%x\n",cycles,e.pia==4?"packet":e.pia==3?"serial-rx":"pia",e.pia==3?e.side:e.pia,e.side,e.value);
        }
        uint64_t before=instructions;
        if(devices) m68k_set_irq(board.irq());
        unsigned elapsed=m68k_execute(1); cycles+=elapsed;
        if(borrowedAddressRegister>=0){m68k_set_reg(m68k_register_t(M68K_REG_A0+borrowedAddressRegister),0);borrowedAddressRegister=-1;}
        if(devices) {board.tick(elapsed);if(board.fault)stop(board.faultReason);}
        if(devices && board.resetRequested) {
            fprintf(events,"watchdog CPU reset instruction=%llu cycles=%llu\n",instructions,cycles);
            context(events);board.reset();cpuReset();
        }
        if(cycles>=nextFrame) {
            frameNumber=uint64_t((long double)cycles*frameHz/hz);nextFrame=uint64_t((long double)(frameNumber+1)*hz/frameHz);
            if(frameEvery && frameNumber%frameEvery==0) {char suffix[64];snprintf(suffix,sizeof suffix,"-frame-%06llu.ppm",frameNumber);writeFrame(out+suffix,compose(board.video));}
            if(window.enabled){if(!window.poll(board))stop("window closed");window.show(compose(board.video),cycles,board.config.cpuHz);}
        }
        if(before==instructions) {if(++inactive>1000) stop("CPU stopped without interrupt source");} else inactive=0;
    }
    window.finishAudio();
    if(audio)wav.close();
    {FILE*f=openfile(out+"-low-accesses.csv","w");fprintf(f,"pc,address,size,direction\n");
     for(auto &a:lowAccesses)fprintf(f,"%06x,%06x,%u,%c\n",std::get<0>(a),std::get<1>(a),std::get<2>(a),std::get<3>(a));fclose(f);}
    {FILE*f=openfile(out+"-rom-writes.csv","w");fprintf(f,"pc,address,size,direction\n");
     for(auto &a:romWrites)fprintf(f,"%06x,%06x,%u,%c\n",std::get<0>(a),std::get<1>(a),std::get<2>(a),std::get<3>(a));fclose(f);}
    if(!ramWriters.empty()){
        FILE*f=openfile(out+"-ram-writers.csv","w");fprintf(f,"byte,pc,address,size,value,instruction\n");
        for(auto &entry:ramWriters){auto &w=entry.second;fprintf(f,"%06x,%06x,%06x,%u,%08x,%llu\n",entry.first,w.pc,w.address,w.size,w.value,w.instruction);}fclose(f);
    }
    if(!saveState.empty()){if(!devices)throw std::runtime_error("state requires --devices");snapshot(saveState,false);}
    if(devices) writeFrame(out+"-final.ppm",compose(board.video));
    {FILE *ram=openfile(out+"-ram.bin","wb");fwrite(memory.data()+0x40000,1,0x40000,ram);fclose(ram);}
    if(!retainedRam.empty() && !board.fault){FILE*f=openfile(retainedRam,"wb");require(fwrite(memory.data()+0x40000,1,0x40000,f)==0x40000,"retained RAM write failed");require(fclose(f)==0,"retained RAM close failed");}
    if(devices && !board.fault){
        pokeri::State state;board.state(state);
        FILE*f=openfile(out+"-board-state.bin","wb");size_t n=fwrite(state.bytes.data(),1,state.bytes.size(),f);int result=fclose(f);
        require(n==state.bytes.size() && !result,"device state output failed");
        std::vector<uint8_t> cpu(pokeri_cpu_state_size());pokeri_cpu_state(cpu.data(),0);
        f=openfile(out+"-cpu-state.bin","wb");n=fwrite(cpu.data(),1,cpu.size(),f);result=fclose(f);
        require(n==cpu.size() && !result,"CPU state output failed");
    }
    FILE*f=openfile(out+"-coverage.bin","wb");fwrite(coverage.data(),1,coverage.size(),f);fclose(f);
    f=openfile(out+"-context.txt","w");
    fprintf(f,"%s\n",stopped?reason.c_str():"budget");context(f);
    fclose(f);fclose(trace);fclose(events);
    if(devices) {
        f=openfile(out+"-nvram.bin","wb");fwrite(board.nvram.bytes.data(),1,0x8000,f);fclose(f);
        f=openfile(out+"-devices.txt","w");fprintf(f,"IRQs=%llu system_edges=%llu input_edges=%llu\n",irqCount,board.systemEdges,board.inputEdges);
        for(unsigned r=0;r<16;++r) fprintf(f,"AY R%u writes=%llu value=%02x\n",r,board.ay.writes[r],board.ay.registers[r]);
        const pokeri::Hd63484 &v=board.video;
        fprintf(f,"HD63484 commands (by opcode group; unexecuted commands=%llu, read-FIFO underflows=%llu):\n",v.unexecuted,v.readUnderflows);
        for(unsigned g=0;g<64;++g) if(v.commands[g]) fprintf(f,"  %-5s %04x-%04x %llu\n",pokeri::Hd63484::mnemonic(g<<10),g<<10,(g<<10)|0x3ff,v.commands[g]);
        fprintf(f,"HD63484 registers:");
        for(unsigned r=2;r<256;++r) if(v.control[r]) fprintf(f," %02x=%02x",r,v.control[r]);
        fprintf(f,"\nHD63484 rwp=%05x origin=%08x\n",v.rwp,v.origin);
        uint64_t videoHash=14695981039346656037ull; unsigned nonzero=0;
        for(uint32_t a=0;a<=v.frameMask;++a) {
            uint16_t word=v.frame[a]; nonzero+=word!=0;
            videoHash=(videoHash^(word>>8))*1099511628211ull;
            videoHash=(videoHash^(word&255))*1099511628211ull;
        }
        fprintf(f,"HD63484 CP=(%d,%d) DP=%04x:%04x VRAM nonzero-words=%u FNV1a64-big-endian=%016llx\n",
                int16_t(v.parameter[0x12]),int16_t(v.parameter[0x13]),v.parameter[0x10],v.parameter[0x11],
                nonzero,static_cast<unsigned long long>(videoHash));
        fclose(f);
    }
    printf("%s: instructions=%llu cycles=%llu PC=%05x; captures %s-*\n",stopped?reason.c_str():"budget",instructions,cycles,m68k_get_reg(nullptr,M68K_REG_PC),out.c_str());
    return stopped?2:0;
} catch(const std::exception &e){fprintf(stderr,"%s\n",e.what());return 1;}
