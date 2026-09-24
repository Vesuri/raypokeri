// Host-only execution and diagnostics. Original ROM instructions are never patched.
#include "m68k.h"
#include "../src/board/Board.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <limits>
#include <string>
#include <stdexcept>
#include <sys/stat.h>
static pokeri::Board board;
static auto &memory = board.memory;
static bool devices;
static uint64_t irqCount;
static std::array<uint8_t, 0x20000> coverage{};
static std::array<uint32_t, 128> history{};
static uint64_t instructions, cycles, lastNew, stallLimit=20000000;
static uint32_t pc;
static unsigned breakpoint=0xffffffff;
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
    address &= 0xfffff;
    uint32_t value=0;
    for (unsigned i=0;i<size;++i) {
        uint32_t a=(address+i)&0xfffff;
        value=(value<<8) | (devices ? board.read8(a) : (a<memory.size()? memory[a]:0));
    }
    if (address+size>memory.size()) {
        if ((!devices || board.fault) && !reported[address]) {fprintf(events,"unmapped read %05x size=%u\n",address,size);context(events);reported[address]=true;}
        fprintf(trace,"%llu,%05x,%05x,%u,R,%08x,%s\n",instructions,pc,address,size,value,devices?board.name(address):"unmapped");
        if (devices ? board.fault : !probe) stop("unmapped read");
    }
    return value;
}
static void writemem(uint32_t address,unsigned size,uint32_t value) {
    address &= 0xfffff;
    for (unsigned i=0;i<size;++i) {
        uint32_t a=(address+i)&0xfffff;
        if(devices) board.write8(a,value>>(8*(size-1-i)));
        else if(a>=0x40000 && a<memory.size()) memory[a]=value>>(8*(size-1-i));
    }
    if(address+size>memory.size()) {
        if ((!devices || board.fault) && !reported[address]) {fprintf(events,"unmapped write %05x size=%u\n",address,size);context(events);reported[address]=true;}
        fprintf(trace,"%llu,%05x,%05x,%u,W,%08x,%s\n",instructions,pc,address,size,value,devices?board.name(address):"unmapped");
        if(devices ? board.fault : !probe) stop("unmapped write");
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
    pc=address&0xfffff;
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
static void resetInstruction() {if(devices) board.reset();}
int main(int argc,char **argv) try {
    uint64_t limit=10000000, cycleLimit=UINT64_MAX; double hz=8000000;
    std::string out="tmp/phase0", rom="rom"; unsigned disasm=0, disasmEnd=0; bool test=false;
    for(int i=1;i<argc;++i) {
        std::string a=argv[i];
        if(a=="--devices") {devices=true;continue;}
        if(a=="--self-test") {test=true;continue;}
        if(a=="--probe") {probe=true;continue;}
        if(a=="--help") {puts("pokeri-host [--instructions N | --ms N] [--clock Hz] [--out tmp/name] [--rom-dir rom] [--probe] [--stall-instructions N] [--break-pc address]\n--devices enables partial portable models; --system-hz N, --input-hz N and --watchdog-ms N enable experimental external signals (default off).\n--probe: Phase 0 logging stubs return zero and continue until stall. Default stops at first unknown access.\nClock defaults to an UNMEASURED 8 MHz; Musashi uses 68000 cycle timing, not 68008 bus timing.");return 0;}
        if(i+1==argc) throw std::runtime_error("missing option value");
        const char *v=argv[++i];
        if(a=="--break-pc") breakpoint=number(v);
        else if(a=="--system-hz") board.config.systemHz=number(v);
        else if(a=="--input-hz") board.config.inputHz=number(v);
        else if(a=="--watchdog-ms") board.config.watchdogMs=number(v);
        else if(a=="--disasm") disasm=number(v);
        else if(a=="--disasm-end") disasmEnd=number(v);
        else if(a=="--stall-instructions") stallLimit=number(v);
        else if(a=="--instructions") limit=number(v);
        else if(a=="--clock") hz=std::stod(v);
        else if(a=="--ms") {cycleLimit=number(v);limit=UINT64_MAX;}
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
    if(disasmEnd>0x100000 || disasm>=0x100000 || (disasmEnd && disasmEnd<=disasm)) throw std::runtime_error("invalid disassembly range");
    if(disasmEnd) {FILE*f=openfile(out+"-disasm.txt","w");for(unsigned a=disasm;a<disasmEnd;) {char line[256];unsigned n=m68k_disassemble(line,a,M68K_CPU_TYPE_68000);fprintf(f,"%05x %s\n",a,line);a+=n;}fclose(f);return 0;}
    trace=openfile(out+"-trace.csv","w");fprintf(trace,"instruction,pc,address,size,direction,value,device\n");
    events=openfile(out+"-events.txt","w");
    board.config.cpuHz=hz;board.log=deviceLog;
    if(board.config.systemHz>1000000 || board.config.inputHz>1000000) throw std::runtime_error("signal frequency too high");
    if(devices) puts("EXPERIMENTAL board model: external signal rates and CPU clock are hypotheses; boot success is not hardware validation.");
    if(devices) { FILE*f=fopen((out+"-nvram.bin").c_str(),"rb");if(f) {require(fread(board.nvram.bytes.data(),1,0x8000,f)==0x8000 && fgetc(f)==EOF,"invalid NVRAM image");fclose(f);} }
    m68k_init();m68k_set_cpu_type(M68K_CPU_TYPE_68000);m68k_set_instr_hook_callback(hook);m68k_set_int_ack_callback(acknowledge);m68k_set_reset_instr_callback(resetInstruction);m68k_pulse_reset();
    uint64_t inactive=0;
    while(!stopped && instructions<limit && cycles<cycleLimit) {
        uint64_t before=instructions;
        if(devices) m68k_set_irq(board.irq());
        unsigned elapsed=m68k_execute(1); cycles+=elapsed;
        if(devices) board.tick(elapsed);
        if(before==instructions) {if(++inactive>1000) stop("CPU stopped without interrupt source");} else inactive=0;
    }
    FILE*f=openfile(out+"-coverage.bin","wb");fwrite(coverage.data(),1,coverage.size(),f);fclose(f);
    f=openfile(out+"-context.txt","w");
    fprintf(f,"%s\n",stopped?reason.c_str():"budget");context(f);
    fclose(f);fclose(trace);fclose(events);
    if(devices) {
        f=openfile(out+"-nvram.bin","wb");fwrite(board.nvram.bytes.data(),1,0x8000,f);fclose(f);
        f=openfile(out+"-devices.txt","w");fprintf(f,"IRQs=%llu system_edges=%llu input_edges=%llu\n",irqCount,board.systemEdges,board.inputEdges);
        for(unsigned r=0;r<16;++r) fprintf(f,"AY R%u writes=%llu value=%02x\n",r,board.ay.writes[r],board.ay.registers[r]);fclose(f);
    }
    printf("%s: instructions=%llu cycles=%llu PC=%05x; captures %s-*\n",stopped?reason.c_str():"budget",instructions,cycles,m68k_get_reg(nullptr,M68K_REG_PC),out.c_str());
    return stopped?2:0;
} catch(const std::exception &e){fprintf(stderr,"%s\n",e.what());return 1;}
