#ifdef POKERI_HOST_ACRTC_TIMING
#include "acrtc_timing_device.h"
#include "acrtc_duration.h"
#include "acrtc_scenario.h"
static bool acrtcDoubleScenario=false,acrtcDoubleAccepted=false;
static pokeri_research::AcrtcScenario acrtcPlayer;
static void acrtcInputRead(unsigned side,uint8_t value,uint8_t mask){acrtcPlayer.read(side,value,mask);}
static uint64_t acrtcFixedCycles=UINT64_MAX,acrtcTableHz=0,acrtcBoardHz=0;
static uint64_t acrtcRawCycles=0,acrtcEstimatedCommands=0,acrtcInferredCommands=0;
static bool acrtcFixedDuration(const pokeri::Hd63484 &v,const std::vector<uint16_t>&words,uint64_t &ticks){
    if(!acrtcTableHz){ticks=acrtcFixedCycles;return ticks!=UINT64_MAX;}
    uint64_t count;bool inferred;
    if(!pokeri_research::AcrtcDuration::counts(v,words,count,inferred))return false;
    if(!pokeri_research::AcrtcDuration::convert(count,acrtcBoardHz,acrtcTableHz,ticks))return false;
    acrtcRawCycles+=count;++acrtcEstimatedCommands;acrtcInferredCommands+=inferred;return true;
}
#endif
#include "../src/native/BootPolicy.h"
#include "../src/native/ShuffleWait.h"
#include "../src/native/ShuffleQueue.h"
#include "../src/Startup.h"
#include "../src/RetainedAccounting.h"
// Host-only reference execution, with explicit relocation and diagnostic-bypass policies.
#include "m68k.h"
#include "VideoOutput.h"
#include "WavOutput.h"
#include "Window.h"
#include "AccessGate.h"
#include "OpcodeAudit.h"
#include "Relocation.h"
#include "Replay.h"
#include "RomIdentity.h"
#include "StartupCache.h"
#include "StartupTiming.h"
#include "../src/board/Board.h"
#include <array>
#include <algorithm>
#include <unordered_map>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cmath>
#include <csignal>
#include <limits>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
struct InputEvent { uint64_t cycle; unsigned pia,side,value; };
static std::vector<InputEvent> inputEvents;
extern "C" unsigned pokeri_cpu_state_size();
extern "C" void pokeri_cpu_state(void*,int);
static pokeri::Board board;
static AccessGate accessGate;
static Relocation relocation;
static ReplayWriter replay;
static int borrowedAddressRegister=-1;
struct RamWriter {uint32_t pc=0,address=0,size=0,value=0;uint64_t instruction=0;};
static std::map<uint32_t,RamWriter> ramWriters;
static RamWriter pendingMovemHalf;
static bool movemHalfPending=false;
static std::set<std::tuple<unsigned,unsigned,unsigned,char>> lowAccesses,romWrites;
static auto &memory = board.memory;
extern "C" unsigned char m68ki_cycles[][0x10000];
static bool devices;
static bool shuffleEnabled=false,shuffleWaiting=false;
static uint64_t shuffleTarget=0,shuffleSteps=0;
static bool shuffleProducer=false,shuffleFrames=false;
static pokeri::ShuffleQueue shuffleQueue;
static std::string shuffleFramePrefix;
static bool captures=true;
static volatile std::sig_atomic_t interrupted=0;
static void interruptPlay(int){interrupted=1;}
static uint64_t irqCount;
static std::array<uint8_t, 0x20000> coverage{};
static std::array<uint32_t, 128> history{};
static uint64_t instructions, cycles, lastNew, stallLimit=20000000;
static uint32_t pc;
static std::string pcHistogramPath,opcodeAuditPath;
static OpcodeAudit opcodeAudit;
static std::unordered_map<uint64_t,uint64_t> pcHistogram;
static unsigned breakpoint=0xffffffff, watchWrite=0xffffffff;
static bool stopped, probe;
static std::string reason;
static FILE *trace, *events;
static std::array<bool, 0x100000> reported{};
static void context(FILE *f);
static void stop(const char *why);
extern "C" void pokeri_exception(unsigned vector) {
    if(captures || vector<25 || vector==31){
        fprintf(events,"exception vector=%u pc=%05x instruction=%llu\n",vector,pc,instructions);
        context(events);
    }
    if(vector<25 || vector==31) stop("CPU exception (vector recorded in events)");
}

static uint32_t shufflePointer(unsigned a){return (uint32_t(memory[a])<<24)|(uint32_t(memory[a+1])<<16)|(uint32_t(memory[a+2])<<8)|memory[a+3];}
static VideoFrame presentationFrame(){
    if(!shuffleQueue.held)return compose(board.video);
    auto ignore=[](unsigned,uint8_t){};
    shuffleQueue.display().exchange(board.video.control,ignore);
    try {auto frame=compose(board.video);shuffleQueue.display().exchange(board.video.control,ignore);return frame;}
    catch(...){shuffleQueue.display().exchange(board.video.control,ignore);throw;}
}
static void captureShuffleFrame(){
    if(shuffleFrames)writeFrame(shuffleFramePrefix+"-shuffle-"+std::to_string(shuffleSteps+1)+".ppm",presentationFrame());
}
static void stop(const char *why) { if (!stopped) reason=why; stopped=true; m68k_end_timeslice(); }
static uint32_t readmem(uint32_t address, unsigned size) {
    if(relocation.enabled && borrowedAddressRegister>=0 && address>=relocation.rom+0x40000 && address-relocation.rom-0x40000<32){
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
    if(address+size>memory.size())replay.bus(instructions,cycles,pc);
    uint32_t value=0;
    for (unsigned i=0;i<size;++i) {
        uint32_t a=(address+i)&0xfffff;
        value=(value<<8) | (devices ? board.read8(a) : (a<memory.size()? memory[a]:0));
    }
    if (address+size>memory.size()) {
        if ((!devices || board.fault) && !reported[address]) {fprintf(events,"unmapped read %05x size=%u\n",address,size);context(events);reported[address]=true;}
        if(trace)fprintf(trace,"%llu,%05x,%05x,%u,R,%08x,%s,%06x\n",instructions,pc,address,size,value,devices?board.name(address):"unmapped",actual);
        if (devices ? board.fault : !probe) stop(devices?board.faultReason:"unmapped read");
    }
    return value;
}
static void writemem(uint32_t address,unsigned size,uint32_t value) {
    // Fast cold boot reaches the existing immutable-ROM marker with A0=0.
    // Admit only this audited write; never map arbitrary old ROM addresses.
    if(pc==0x2358 && address==0x91 && size==1 && value==0xc3){romWrites.emplace(pc,address,size,'W');return;}
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
    if(address+size>memory.size())replay.bus(instructions,cycles,pc);
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
        if(trace)fprintf(trace,"%llu,%05x,%05x,%u,W,%08x,%s,%06x\n",instructions,pc,address,size,value,devices?board.name(address):"unmapped",actual);
        if(devices ? board.fault : !probe) stop(devices?board.faultReason:"unmapped write");
    }
}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return readmem(a,1);}
unsigned m68k_read_memory_16(unsigned a){
    // Authored BRA.s-to-self while waiting. The ROM bytes remain immutable;
    // Musashi still executes instructions and services its ordinary IRQs.
    if(shuffleWaiting && pc==pokeri::ShuffleWait::pc && relocation.canonical(a)==pc)return 0x60fe;
    return readmem(a,2);
}
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
#ifdef POKERI_HOST_ACRTC_TIMING
    if(captures && board.timedVideo){
        static bool inside=false;static uint64_t startWords=0,startCycle=0;
        if(pc==0x2e26){
            if(inside){stop("timing observer: nested video handler");return;}
            inside=true;startWords=board.timedVideo->wordsWritten;startCycle=cycles;
        }
        if(pc==0x2e8a && inside){
            if(memory[pc]!=0x4e || memory[pc+1]!=0x73){stop("timing observer: video return guard");return;}
            fprintf(events,"acrtc-service cycle=%llu words=%llu elapsed=%llu\n",cycles,
                    (unsigned long long)(board.timedVideo->wordsWritten-startWords),cycles-startCycle);
            inside=false;
        }
    }
#endif
    if(shuffleEnabled && !shuffleProducer){
        if(pc==pokeri::ShuffleWait::pc){
            uint32_t caller=relocation.canonical(readmem(m68k_get_reg(nullptr,M68K_REG_SP),4));
            if(!pokeri::ShuffleWait::caller(caller)){stop("shuffle marker caller outside verified loop");return;}
            if((m68k_get_reg(nullptr,M68K_REG_SR)&0x700)>=0x500){stop("shuffle marker with board IRQs masked");return;}
            if(captures)fprintf(events,"shuffle marker producer=%x consumer=%x begin=%x end=%x\n",shufflePointer(0x41326),shufflePointer(0x4132a),shufflePointer(0x413be),shufflePointer(0x413c2));
            if(!shuffleQueue.mark(shufflePointer(0x41326),shufflePointer(0x4132a),shufflePointer(0x413be),shufflePointer(0x413c2),board.video.control)){
                stop(shuffleQueue.error);return;
            }
        }
        if(pc==0x2e62)shuffleQueue.consume(m68k_get_reg(nullptr,M68K_REG_A1));
        board.video.presentationBusy=shuffleQueue.held;
    }
    if(shuffleEnabled && shuffleProducer && pc==pokeri::ShuffleWait::pc){
        uint32_t sp=m68k_get_reg(nullptr,M68K_REG_SP),caller=relocation.canonical(readmem(sp,4));
        if(!pokeri::ShuffleWait::caller(caller)){stop("shuffle wait caller outside verified loop");return;}
        if((m68k_get_reg(nullptr,M68K_REG_SR)&0x700)>=0x500){stop("shuffle wait with board interrupts masked");return;}
        if(!shuffleWaiting){shuffleWaiting=true;shuffleTarget=0;}
        if(!shuffleTarget && pokeri::ShuffleWait::drained(memory) && (board.video.statusNow()&pokeri::Hd63484::CED))
        {
            captureShuffleFrame();
            shuffleTarget=(cycles/(board.config.cpuHz/50)+1)*(board.config.cpuHz/50);
        }
        if(shuffleTarget && cycles>=shuffleTarget){
            shuffleWaiting=false;shuffleTarget=0;++shuffleSteps;
            if(captures)fprintf(events,"shuffle boundary=%llu cycles=%llu irqs=%llu\n",shuffleSteps,cycles,irqCount);
        }
    }
    auto low=relocation.lowHooks.find(pc);
    if(relocation.enabled && low!=relocation.lowHooks.end() && m68k_get_reg(nullptr,m68k_register_t(M68K_REG_A0+low->second.reg))==0){
        borrowedAddressRegister=low->second.reg;
        m68k_set_reg(m68k_register_t(M68K_REG_A0+borrowedAddressRegister),relocation.rom+0x40000);
    }
    auto control=relocation.controls.find(pc);
    if(relocation.enabled && control!=relocation.controls.end() && control->second.kind=="set_ram_delta_d7")
        m68k_set_reg(M68K_REG_D7,relocation.ram-0x40000);
    if(!pcHistogramPath.empty())++pcHistogram[((cycles/board.config.cpuHz)<<32)|pc];
    if(!opcodeAuditPath.empty()){
        if(pc+1>=memory.size()){stop("opcode audit fetch bounds");return;}
        opcodeAudit.observe(pc,uint16_t((memory[pc]<<8)|memory[pc+1]));
    }
    history[instructions++%history.size()]=pc;
    if(!(coverage[pc>>3]&(1<<(pc&7)))) lastNew=instructions;
    coverage[pc>>3]|=1<<(pc&7);
    if(pc==breakpoint) {context(events);stop("breakpoint");}
    if(devices && pc==0x24fa) {context(events);stop("ROM fatal startup error (D0 in context)");}
    if(stallLimit && instructions-lastNew>stallLimit) stop("stall: no new PC within instruction window");
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
    relocation.enabled=relocation.bypass=true;relocation.rom=0x100000;relocation.ram=0x140000;relocation.guard=0x180000;
    relocation.validate();stopped=false;
    memory[0x40000]=0x12;memory[0x40001]=0x34;memory[0x40002]=0x56;memory[0x40003]=0x78;
    require(readmem(relocation.ram,4)==0x12345678 && !stopped,"adjacent RAM is not a low-vector alias");
    pc=0x900;relocation.lowHooks.emplace(pc,Relocation::LowHook{0,0,4});
    relocation.vectorShadow[0]=0xab;borrowedAddressRegister=0;
    require(readmem(relocation.rom+0x40000,4)==0xab000000 && !stopped,"guarded sentinel reads original vector at adjacent placement");
    borrowedAddressRegister=-1;
    require(readmem(relocation.ram,4)==0x12345678 && !stopped,"sentinel redirect ends with hook");
    puts("PASS: ROM write protection, RAM endianness, 20-bit mask/wrap, unknown-access stops, instruction count, privilege exception, coverage");
}
static int acknowledge(int level) {++irqCount;unsigned vector=level==5?board.vector():24+level;
#ifdef POKERI_HOST_ACRTC_TIMING
    if(vector==0x40 && captures)fprintf(events,"acrtc-irq cycle=%llu words=%llu\n",cycles,(unsigned long long)board.timedVideo->wordsWritten);
#endif
replay.event(2,instructions,cycles,relocation.canonical(m68k_get_reg(nullptr,M68K_REG_PC)),level,vector);return level==5?vector:M68K_INT_ACK_AUTOVECTOR;}
static void deviceLog(const char *name,unsigned reg,uint8_t value) {fprintf(events,"%s register=%u value=%02x pc=%05x instruction=%llu cycle=%llu\n",name,reg,value,pc,instructions,cycles);}
static uint64_t videoLogged;
static FILE *videoCatalog=nullptr;
// Every HD63484 command, up to a cap; the full counts go to <out>-devices.txt.
static void videoCommand(const uint16_t *w,unsigned n,bool executed) {
    if(videoCatalog){
        fprintf(videoCatalog,"V %llu %llu %u",cycles,instructions,unsigned(executed));
        for(unsigned i=0;i<n;++i)fprintf(videoCatalog," %04x",w[i]);
        fputc('\n',videoCatalog);
        unsigned group=w[0]>>10;
        if(executed && group>=56 && n==5){
            unsigned direction=(w[0]>>8)&15,width=unsigned(std::abs(int16_t(w[3])))+1,height=unsigned(std::abs(int16_t(w[4])))+1;
            int x=int16_t(board.video.parameter[18]),y=int16_t(board.video.parameter[19]);
            if(direction==12)x=int16_t(x-int(width));else y=int16_t(y+(direction==3?int(height):-int(height)));
            fprintf(videoCatalog,"C %llu %04x %d %d %d %d %u %u\n",cycles,w[0],int16_t(w[1]),int16_t(w[2]),x,y,width,height);
        }
        if(executed && group>=52 && group<=55 && n==2)
            fprintf(videoCatalog,"T %llu %04x %d %d %u %u\n",cycles,w[0],int16_t(board.video.parameter[18]),int16_t(board.video.parameter[19])-int((w[1]>>8)+1),(w[1]&255)+1,(w[1]>>8)+1);
        // Capture the destination before the first drawing command. These five
        // WPRs and AMOVE only change semantic state. The catalog tool validates
        // the entire subsequent recipe before using this background sample.
        static unsigned prefix=0;
        static const uint16_t prefixOps[]={0x0800,0x0805,0x0806,0x0807,0x0801};
        if(prefix==5 && w[0]==0x8000 && (board.video.control[2]&7)==2){
            const auto &v=board.video;
            unsigned dn=v.origin>>30,a=0xc2+8*dn;
            int mw=((unsigned(v.control[a])<<8)|v.control[a+1])&4095;
            int x=int16_t(w[1]),y=int16_t(w[2]);
            fputs("P ",videoCatalog);
            for(int row=0;row<100;++row)for(int col=0;col<88;++col){
                int dot=int16_t(x+col)+int((v.origin&15)>>2);
                int word=dot>=0?dot/4:-int((unsigned(-dot)+3)/4);
                uint32_t address=uint32_t((v.origin>>4)&0xfffff)+uint32_t(word)-uint32_t(int16_t(y+row)*mw);
                unsigned color=(v.readWord(address)>>((unsigned(dot)&3)*4))&15;
                fputc("0123456789abcdef"[color],videoCatalog);
            }
            fputc('\n',videoCatalog);
        }
        if(prefix<5 && w[0]==prefixOps[prefix])++prefix;
        else prefix=w[0]==prefixOps[0]?1:0;
        if(w[0]==0x0800){const auto &v=board.video;
            fprintf(videoCatalog,"S %08x %05x %05x %02x",v.origin,v.frameMask,v.rwp,v.status);
            for(auto x:v.parameter)fprintf(videoCatalog," %04x",x);
            for(auto x:v.pattern)fprintf(videoCatalog," %04x",x);
            for(auto x:v.control)fprintf(videoCatalog," %02x",x);
            fputc('\n',videoCatalog);
        }
    }
    if(!events)return;
    if(++videoLogged>20000 && !board.video.error) return;
    fprintf(events,"HD63484 %-5s%s pc=%05x instruction=%llu words=",pokeri::Hd63484::mnemonic(w[0]),executed?"":" (not executed)",pc,instructions);
    for(unsigned i=0;i<n && i<12;++i) fprintf(events,"%s%04x",i?" ":"",w[i]);
    fprintf(events,"%s\n",n>12?" ...":"");
}
static void cpuReset() {
    shuffleWaiting=false;shuffleTarget=0;shuffleQueue.reset();board.video.presentationBusy=false;
    relocation.resetVectors=true;m68k_pulse_reset();relocation.resetVectors=false;
    // Host virtual exception vector base; Phase 4 supplies synthetic exceptions.
    if(relocation.enabled)m68k_set_reg(M68K_REG_VBR,relocation.rom);
}
static void resetInstruction() {
    if(relocation.enabled && !relocation.resetHooks.count(pc)){context(events);stop("RESET outside hook table");return;}
    replay.event(6,instructions,cycles,pc);
    if(devices)board.reset();
}
int main(int argc,char **argv) try {
    startupTiming("entered main");
    uint64_t limit=10000000, cycleLimit=UINT64_MAX,budgetMs=UINT64_MAX; double hz=8000000;
    std::string out="tmp/phase0", rom="rom", inputPath,saveState,loadState,retainedRam,accountingPath,codeMap,relocTable="host/tables/relocations.csv",lowHookTable="host/tables/low-vector-hooks.csv",controlTable="host/tables/control-hooks.csv",resetTable="host/tables/reset-hooks.csv",provenancePath,replayPath,videoCatalogPath; unsigned disasm=0, disasmEnd=0; bool test=false,audio=false,liveAudio=false,windowRequested=false;int paletteBank=-1; unsigned frameEvery=0,frameHz=50;uint64_t nextFrame=0,frameNumber=0;
    bool play=Window::available(),captureFrames=false,userQuit=false;
    bool exportCycles=false;
    bool cacheEligible=true,coldBoot=false,warmStart=false,cachePending=false;
    for(int i=1;i<argc;++i)if(std::string(argv[i])=="--research")play=false;
    bool diagnosticDisplayDelays=false;
    bool skipHardwareTests=play,autoSetup=play;shuffleEnabled=play;
    if(play){
        std::setvbuf(stdout,nullptr,_IOLBF,0);std::signal(SIGINT,interruptPlay);
        devices=windowRequested=liveAudio=board.peer.enabled=true;
        limit=UINT64_MAX;stallLimit=0;captures=false;out="tmp/pokeri";paletteBank=0;
        board.config.systemHz=100;board.config.inputHz=50;
        board.config.watchdogMs=400;board.config.watchdogResetUs=50000;board.ay.clockHz=1000000;
        struct stat info;if(stat("rom/77POK30",&info))rom=Window::defaultRomDirectory();
    }
    for(int i=1;i<argc;++i) {
        std::string a=argv[i];
        if(a!= "--mute" && a!="--ms" && a!="--instructions" && a!="--frames" && a!="--wav" && a!="--save-state" && a!="--rom-dir" && a!="--cold-boot" && a!="--shuffle-vblank" && a!="--no-shuffle-vblank")cacheEligible=false;
#ifdef POKERI_HOST_ACRTC_TIMING
        if(a=="--acrtc-double-scenario"){acrtcDoubleScenario=true;continue;}
#endif
        if(a=="--opcode-cycles"){exportCycles=true;continue;}
        if(a=="--shuffle-vblank"){shuffleEnabled=true;shuffleProducer=false;continue;}
        if(a=="--shuffle-producer-vblank"){shuffleEnabled=true;shuffleProducer=true;continue;}
        if(a=="--shuffle-frames"){shuffleFrames=true;continue;}
        if(a=="--no-shuffle-vblank"){shuffleEnabled=false;continue;}
        if(a=="--auto-setup"){autoSetup=true;continue;}
        if(a=="--skip-hardware-tests"){skipHardwareTests=true;continue;}
        if(a=="--diagnostic-display-delays"){diagnosticDisplayDelays=true;continue;}
        if(a=="--hardware-tests"){skipHardwareTests=false;continue;}
        if(a=="--cold-boot"){coldBoot=true;continue;}
        if(a=="--research")continue;
        if(a=="--mute"){liveAudio=false;continue;}
        if(a=="--capture"){captures=true;continue;}
        if(a=="--frames"){captureFrames=true;continue;}
        if(a=="--bypass-module-checksums") {relocation.bypass=true;continue;}
        if(a=="--window") {windowRequested=true;continue;}
        if(a=="--live-audio") {liveAudio=true;continue;}
        if(a=="--wav") {audio=true;continue;}
        if(a=="--serial-peer") {board.peer.enabled=true;continue;}
        if(a=="--devices") {devices=true;continue;}
        if(a=="--self-test") {test=true;continue;}
        if(a=="--probe") {probe=true;continue;}
#ifdef POKERI_HOST_ACRTC_TIMING
        if(a=="--help")puts("Timing research build: requires --devices and exactly one of --acrtc-fixed-cycles N (synthetic cycles/command) or --acrtc-table-hz N (inferred table-cycle rate, curve geometry and PAINT scan-run estimate). Neither is hardware calibration. --acrtc-double-scenario runs the shared external player after fresh --auto-setup at 8 MHz. No snapshots, replay, relocation, window or shuffle pacing.");
#endif
        if(a=="--help") {if(Window::available())puts("SDL defaults: zero player credits, live audio, no captures, no time limit.\n--cold-boot rebuilds the local clean-start cache; hardware diagnostics are skipped.\n--auto-setup enables acknowledgement-driven cabinet setup in research mode.\n--shuffle-vblank / --no-shuffle-vblank enables/disables consumer-paced shuffle (on for normal play, off for research).\n--shuffle-producer-vblank selects the legacy comparison; --shuffle-frames captures each step under tmp/.\n--diagnostic-display-delays retains old digit dwells for historical replay comparison.\n--hardware-tests restores coin-op tests; --skip-hardware-tests enables fast startup in research mode.\n--ms N / --instructions N limit play after automatic setup (or snapshot restore).\n--mute silences playback; --frames saves a final frame; --capture / --out PREFIX enable diagnostics.\n--research restores the original harness defaults and absolute budgets.\nSpace deal/draw, B bet, 1–5 hold, Return collect, D double, arrows big/small, C coin, Esc quit.");puts("pokeri-host [--instructions N | --ms N] [--clock Hz] [--out tmp/name] [--rom-dir rom] [--probe] [--stall-instructions N] [--break-pc address]\n--devices enables partial portable models; --system-hz N, --input-hz N and --watchdog-ms N enable experimental external signals (default off).\n--video-kwords N: installed HD63484 memory in K words (power of two; default 256 = 512 KB, the target variant; 1024 = 2 MB).\n--probe: Phase 0 logging stubs return zero and continue until stall. Default stops at first unknown access.\n--watchdog-reset-us N: explicit reset delay after warning (research profile: 50000).\n--inputs PATH: absolute-time PIA/serial input script; --serial-peer enables the coin/meter peripheral model.\n--video-catalog tmp/file: complete command words and WPR0 contexts for offline asset cataloging.\n--frame-every N --frame-hz N: periodic PPM capture; default cadence hypothesis 50 Hz. Final frame saved with diagnostics or --frames.\n--palette-rom 0..3: test the ROM RAMDAC palette at runtime; default is labelled placeholder.\n--ay-clock Hz --wav: explicit AY oscillator hypothesis and mono 44100 Hz WAV capture.\n--save-state tmp/file --load-state tmp/file: full instruction-boundary state, same ROM/core ABI.\n--retained-ram tmp/file: experimental full main-RAM retention across a fresh CPU boot.\n--accounting-ram tmp/file: retain the verified accounting block; use --auto-setup for cold/warm cabinet setup.\n--window: SDL build only (make harness SDL=1, build/pokeri-host-sdl).\n--live-audio: play AY sound with --window; requires an AY clock (explicit or restored). May be combined with --wav.\n--bypass-module-checksums: explicit temporary bypass after verifying all four SHA-256 hashes.\n--rom-base N --ram-base N --device-base N: strict 24-bit relocated mode, old address ranges unmapped.\n--relocation-table CSV --low-vector-hooks CSV --control-hooks CSV --reset-hooks CSV: explicit patch/hook metadata.\n--pc-histogram tmp/file.csv: instruction counts by PC and reference board-second.\n--opcode-audit tmp/file.csv: observed PC/opcode counts and 68060 MOVEP classification (host research).\n--ram-provenance PATH: preserve last-writer evidence for selected RAM bytes across checkpoints.\n--record-replay tmp/file: cold-boot diagnostic timing and external-input capture (requires checksum bypass).\n--code-map COVERAGE: export covered ROM instruction lengths for research.\n--io-table CSV: reject hardware accesses outside the audited PC/address/size/direction table.\nBudgets are absolute emulated endpoints, including after restore. Clock defaults to UNMEASURED 8 MHz; Musashi uses 68000 cycle timing, not 68008 bus timing.");return 0;}
        if(i+1==argc) throw std::runtime_error("missing option value");
        const char *v=argv[++i];
#ifdef POKERI_HOST_ACRTC_TIMING
        if(a=="--acrtc-table-hz"){acrtcTableHz=number(v);continue;}
        if(a=="--acrtc-fixed-cycles"){acrtcFixedCycles=number(v);continue;}
#endif
        if(a=="--video-catalog") videoCatalogPath=v;
        else if(a=="--record-replay") replayPath=v;
        else if(a=="--break-pc") breakpoint=number(v);
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
        else if(a=="--opcode-audit") {opcodeAuditPath=v;if(opcodeAuditPath.compare(0,4,"tmp/") || opcodeAuditPath.find("..")!=std::string::npos)throw std::runtime_error("opcode audit must be under tmp/");}
        else if(a=="--pc-histogram") {pcHistogramPath=v;if(pcHistogramPath.compare(0,4,"tmp/") || pcHistogramPath.find("..")!=std::string::npos)throw std::runtime_error("PC histogram must be under tmp/");}
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
        else if(a=="--accounting-ram") accountingPath=v;
        else if(a=="--save-state") saveState=v;
        else if(a=="--load-state") loadState=v;
        else if(a=="--out") {out=v;captures=true;}
        else if(a=="--rom-dir") rom=v;
        else throw std::runtime_error("unknown option "+a);
    }
#ifdef POKERI_HOST_ACRTC_TIMING
    if((acrtcFixedCycles==UINT64_MAX)==!acrtcTableHz || acrtcTableHz>1000000000 || !devices || play || shuffleEnabled || windowRequested ||
       !loadState.empty() || !saveState.empty() || !replayPath.empty() || relocation.enabled)
        throw std::runtime_error("timing research requires --devices and exactly one of --acrtc-fixed-cycles N or --acrtc-table-hz N; snapshots, replay, relocation, window and shuffle pacing are unsupported");
    if(!std::isfinite(hz) || hz<1 || hz>1000000000)throw std::runtime_error("invalid timing research board clock");
    if(acrtcDoubleScenario && (hz!=8000000 || !autoSetup || !inputPath.empty() || !retainedRam.empty() || !accountingPath.empty()))
        throw std::runtime_error("Double timing scenario requires fresh auto-setup, 8 MHz and no input script/accounting");
    acrtcBoardHz=uint64_t(hz);
    pokeri_research::AcrtcTimingDevice timedVideo(board.video,acrtcFixedDuration);
    board.timedVideo=&timedVideo;board.checkTimedVideo();
    if(board.fault)throw std::runtime_error(board.faultReason);
    if(acrtcTableHz)fprintf(stderr,"INFERRED ACRTC timing: %llu table cycles/s; renderer curve dots and PAINT scan-run approximation; not physical calibration.\n",(unsigned long long)acrtcTableHz);
    else fprintf(stderr,"SYNTHETIC ACRTC timing: %llu board cycles per command; not physical calibration.\n",(unsigned long long)acrtcFixedCycles);
#endif
    if(skipHardwareTests){
        if(board.video.frameMask!=0x3ffff)throw std::runtime_error("fast startup supports the configured 512 KB video board only");
        relocation.bypass=true;
    }
    if(!std::isfinite(hz) || hz<1 || hz>1000000000) throw std::runtime_error("clock must be between 1 and 1000000000 Hz");
    if(cycleLimit!=UINT64_MAX) {
        long double n=cycleLimit*(long double)hz/1000;
        if(n>=static_cast<long double>(UINT64_MAX)) throw std::runtime_error("time budget overflow");
        cycleLimit=uint64_t(n);
    }
    if(out.compare(0,4,"tmp/") || out.find("..")!=std::string::npos) throw std::runtime_error("output must be under tmp/");
    mkdir("tmp",0755);
    if(exportCycles){m68k_init();FILE *f=openfile("tmp/m68000-cycles.bin","wb");require(fwrite(m68ki_cycles[0],1,65536,f)==65536,"opcode cycle export failed");fclose(f);return 0;}
    if(test) {trace=openfile("tmp/selftest-trace.csv","w");events=openfile("tmp/selftest-events.txt","w");selftest();fclose(trace);fclose(events);return 0;}
    // Address order, NOT name order: 30 at $00000, 38 at $10000, 34 at $20000 (docs/rom-set.md —
    // the ROM's own module checksum passes only in this order).
    const char *chips[]={"77POK30","77POK38","77POK34","PARA200J"};
    for(unsigned i=0;i<4;++i) {FILE*f=openfile(rom+"/"+chips[i],"rb");size_t n=fread(memory.data()+i*65536,1,65536,f);int extra=fgetc(f);fclose(f);if(n!=65536 || extra!=EOF) throw std::runtime_error("wrong ROM size");}
    if(shuffleFrames && (out.compare(0,4,"tmp/") || out.find("..")!=std::string::npos))throw std::runtime_error("shuffle frames must be under tmp/");
    shuffleFramePrefix=out;
    if(shuffleEnabled || play || accessGate.active() || relocation.enabled || relocation.bypass)verifyRomIdentity(memory.data());
    if(shuffleEnabled && (memory[pokeri::ShuffleWait::pc]!=0x4e || memory[pokeri::ShuffleWait::pc+1]!=0x75))throw std::runtime_error("shuffle RTS guard mismatch");
    startupTiming("ROM files loaded and verified");
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
    if(relocation.enabled || (relocation.bypass && !skipHardwareTests))relocation.loadControls(controlTable);
    if(relocation.enabled){relocation.loadLowHooks(lowHookTable);relocation.loadResetHooks(resetTable);}
    relocation.patch(memory,relocTable);
    if(skipHardwareTests)pokeri::applyBootPolicy(memory.data(),!diagnosticDisplayDelays);
    if(captures)trace=openfile(out+"-trace.csv","w");if(trace)fprintf(trace,"instruction,pc,address,size,direction,value,device,cpu_address\n");
    events=captures?openfile(out+"-events.txt","w"):stderr;
    if(!videoCatalogPath.empty()){
        if(videoCatalogPath.compare(0,4,"tmp/") || videoCatalogPath.find("..")!=std::string::npos)throw std::runtime_error("video catalog must be under tmp/");
        videoCatalog=openfile(videoCatalogPath,"w");board.video.commandLog=videoCommand;
    }
    board.config.cpuHz=hz;if(captures){board.log=deviceLog;board.video.commandLog=videoCommand;}
    if(board.config.systemHz>1000000 || board.config.inputHz>1000000) throw std::runtime_error("signal frequency too high");
    if(devices && !play) puts("EXPERIMENTAL board model: external signal rates and CPU clock are hypotheses; boot success is not hardware validation.");
    if(devices && captures) { FILE*f=fopen((out+"-nvram.bin").c_str(),"rb");if(f) {require(fread(board.nvram.bytes.data(),1,0x8000,f)==0x8000 && fgetc(f)==EOF,"invalid NVRAM image");fclose(f);} }
    if(!retainedRam.empty()) {
        if(retainedRam.compare(0,4,"tmp/") || retainedRam.find("..")!=std::string::npos)throw std::runtime_error("retained RAM must be under tmp/");
        if(!loadState.empty())throw std::runtime_error("choose retained RAM cold boot or full snapshot restore");
        FILE*f=fopen(retainedRam.c_str(),"rb");if(f){require(fread(memory.data()+0x40000,1,0x40000,f)==0x40000 && fgetc(f)==EOF,"invalid retained RAM image");fclose(f);}else if(errno!=ENOENT)throw std::runtime_error("cannot read retained RAM image");
    }
    bool accountingLoaded=false;
    if(!accountingPath.empty()){
        if(accountingPath.compare(0,4,"tmp/") || accountingPath.find("..")!=std::string::npos)throw std::runtime_error("accounting file must be under tmp/");
        if(!loadState.empty() || !retainedRam.empty())throw std::runtime_error("accounting boot conflicts with another restore policy");
        FILE*f=fopen(accountingPath.c_str(),"rb");
        if(f){pokeri::RetainedAccounting image;bool okay=fread(image.bytes.data(),1,image.bytes.size(),f)==image.bytes.size() && fgetc(f)==EOF;fclose(f);
            require(okay,"invalid retained accounting image");
            if(!image.isFresh()){require(image.decode(memory.data()),"invalid retained accounting image");accountingLoaded=true;}
        }else if(errno!=ENOENT)throw std::runtime_error("cannot read accounting image");
    }

    m68k_init();m68k_set_cpu_type(M68K_CPU_TYPE_68000);m68k_set_instr_hook_callback(hook);m68k_set_int_ack_callback(acknowledge);m68k_set_reset_instr_callback(resetInstruction);cpuReset();
    startupTiming("CPU initialized");
    if(!frameHz || frameHz>1000) throw std::runtime_error("invalid frame frequency");
    nextFrame=uint64_t(hz)/frameHz;
    size_t nextInput=0;
    uint64_t inactive=0;
    std::string startupCache=play && cacheEligible && accountingPath.empty()?startupCachePath(rom):"";
    auto snapshot=[&](const std::string &path,bool reading,bool internal=false){
        if(!internal && (path.compare(0,4,"tmp/") || path.find("..")!=std::string::npos))throw std::runtime_error("state must be under tmp/");
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
        // Optional extension keeps existing research snapshots readable. New
        // snapshots preserve waits even when saved inside an interrupt handler.
        if((!reading && shuffleEnabled && !internal) || (reading && s.cursor<s.bytes.size())){
            uint32_t tag=0x53485632;bool enabled=shuffleEnabled;
            s.fields(tag,enabled,shuffleWaiting,shuffleTarget,shuffleSteps);
            if(tag!=0x53485631 && tag!=0x53485632)throw std::runtime_error("unknown snapshot timing extension");
            bool producer=tag==0x53485631?true:shuffleProducer;
            if(tag==0x53485632)s.value(producer);
            if(enabled && producer!=shuffleProducer)throw std::runtime_error("snapshot shuffle producer/consumer policy mismatch");
            if(tag==0x53485632 && !producer)shuffleQueue.state(s);
            if(reading)board.video.presentationBusy=shuffleQueue.held;
            if(reading && enabled!=shuffleEnabled)throw std::runtime_error("snapshot shuffle timing policy mismatch");
        }
        nextInput=inputIndex;
        if(reading){if(s.cursor!=s.bytes.size())throw std::runtime_error("trailing state data");pokeri_cpu_state(cpu.data(),1);hz=board.config.cpuHz;}
        else {FILE*f=openfile(path,"wb");size_t n=fwrite(s.bytes.data(),1,s.bytes.size(),f);int result=fclose(f);if(n!=s.bytes.size()||result)throw std::runtime_error("cannot write state");}
    };
    if(!startupCache.empty() && !coldBoot){struct stat info;if(!stat(startupCache.c_str(),&info)){snapshot(startupCache,true,true);warmStart=true;}}
    if(!loadState.empty()){if(!devices)throw std::runtime_error("state requires --devices");snapshot(loadState,true);if(budgetMs!=UINT64_MAX){long double n=budgetMs*(long double)hz/1000;if(n>=static_cast<long double>(UINT64_MAX))throw std::runtime_error("time budget overflow");cycleLimit=uint64_t(n);}}
    if(!frameHz || frameHz>1000 || frameHz>hz)throw std::runtime_error("invalid restored frame frequency");
    if(shuffleEnabled && (frameHz!=50 || board.config.cpuHz<50 || board.config.cpuHz%50))throw std::runtime_error("shuffle waits require 50 Hz presentation and an integral cycles-per-frame clock");
    if(!inputPath.empty()) {
        inputEvents.clear();nextInput=0;
        std::ifstream in(inputPath); if(!in)throw std::runtime_error("cannot open input script");
        std::string line;
        while(std::getline(in,line)) {
            line=line.substr(0,line.find('#'));if(line.find_first_not_of(" \t\r")==std::string::npos)continue;
            std::istringstream row(line);std::string ms,pia,side,value,extra;
            if(!(row>>ms>>pia>>side>>value) || (row>>extra))throw std::runtime_error("input row: milliseconds pia side value");
            size_t timeEnd=0;long double timeMs=std::stold(ms,&timeEnd);
            if(timeEnd!=ms.size() || !std::isfinite(timeMs) || timeMs<0)throw std::runtime_error("invalid input time");
            long double when=timeMs*(long double)hz/1000;if(when>=static_cast<long double>(UINT64_MAX))throw std::runtime_error("input time overflow");
            if(number(side.c_str())>UINT32_MAX || number(value.c_str())>UINT32_MAX)throw std::runtime_error("input value overflow");
            InputEvent e{uint64_t(when),pia=="packet"?4u:pia=="rx"?3u:unsigned(number(pia.c_str())),unsigned(number(side.c_str())),unsigned(number(value.c_str()))};
            if(e.pia>4 || (e.pia==4?(e.side>63 || e.value>0x2ffff):(e.side>(e.pia==3?0u:1u) || e.value>255)) || (!inputEvents.empty() && e.cycle<inputEvents.back().cycle))throw std::runtime_error("invalid input range/order");
            if(e.pia==4 && !board.peer.enabled)throw std::runtime_error("packet input requires --serial-peer");
            if(inputEvents.size()>=1000000)throw std::runtime_error("too many input events");
            inputEvents.push_back(e);
        }
    }
    bool preparing=autoSetup && !warmStart && loadState.empty() && inputPath.empty() && retainedRam.empty();
    uint64_t runStartCycles=cycles,runStartInstructions=instructions,nextSetupCycle=cycles;
    pokeri::Startup startup;startup.retained=accountingLoaded;
    if(preparing){
        inputEvents.push_back({cycles,1,0,0xff});inputEvents.push_back({cycles,1,1,0x7f});inputEvents.push_back({cycles,2,0,8});
        puts("Preparing Pokeri…");
    }
    if(liveAudio && !windowRequested)throw std::runtime_error("--live-audio requires --window");
    if((audio || liveAudio) && !board.ay.clockHz)throw std::runtime_error("audio requires --ay-clock or a snapshot with an AY clock");
    if(!replayPath.empty() && shuffleEnabled)throw std::runtime_error("shuffle waits are a live presentation policy; disable for diagnostic replay");
    if(!replayPath.empty()){
        if(replayPath.compare(0,4,"tmp/") || replayPath.find("..")!=std::string::npos)throw std::runtime_error("replay must be under tmp/");
        if(!loadState.empty() || !retainedRam.empty() || !accountingPath.empty() || relocation.enabled)throw std::runtime_error("replay requires cold boot at reference addresses");
        if(!devices || !relocation.bypass)throw std::runtime_error("replay requires devices and checksum bypass");
        for(auto v:board.nvram.bytes)if(v)throw std::runtime_error("replay requires zero initial NVRAM");
        replay.open(replayPath);
        uint32_t settings[]={board.config.cpuHz,board.config.systemHz,board.config.inputHz,board.config.watchdogMs,board.config.watchdogResetUs,board.ay.clockHz,board.video.frameMask,board.video.wptnCountsBytes,board.peer.enabled,skipHardwareTests?(diagnosticDisplayDelays?3u:7u):1u};
        for(unsigned i=0;i<10;++i)replay.event(7,0,0,i,settings[i]);
    }
    startupTiming("state/setup ready; opening SDL");
    Window window;if(windowRequested)window.open(cycles);
    if(liveAudio && !preparing)window.openAudio();
    if(windowRequested && !preparing)window.ready(cycles);
    if(warmStart)puts("Ready. Zero credits; C: coin; Space: deal/draw; Esc: quit.");
    WavOutput wav;if(audio)wav.open(out+".wav");
    struct AudioOutput : pokeri::Tone {
        WavOutput *wav=nullptr;Window *window=nullptr;
        void sample(int16_t value) override {if(wav)wav->sample(value);if(window)window->sample(value);}
    } output;
    output.wav=audio?&wav:nullptr;output.window=liveAudio && !preparing?&window:nullptr;
    if(audio || liveAudio)board.ay.sink=&output;
    while(nextInput<inputEvents.size() && inputEvents[nextInput].cycle<cycles)++nextInput;
    while(!stopped && !userQuit && !interrupted && (preparing || ((instructions-(play?runStartInstructions:0))<limit && (cycles-(play?runStartCycles:0))<cycleLimit))) {
        while(nextInput<inputEvents.size() && inputEvents[nextInput].cycle<=cycles) {
            auto e=inputEvents[nextInput++];replay.event(3,instructions,cycles,relocation.canonical(m68k_get_reg(nullptr,M68K_REG_PC)),(e.pia<<16)|e.side,e.value);if(e.pia==4){std::vector<uint8_t> p{uint8_t(e.side)};unsigned n=e.value>>16;if(n>2)throw std::runtime_error("packet payload length");if(n==2)p.push_back(e.value>>8);if(n)p.push_back(e.value);board.peer.enqueue(p);}else if(e.pia==3)board.serial[e.side].receive.push_back(e.value);else board.pia[e.pia].input[e.side]=e.value;
            if(captures)fprintf(events,"input cycle=%llu kind=%s device=%u register=%u value=%x\n",cycles,e.pia==4?"packet":e.pia==3?"serial-rx":"pia",e.pia==3?e.side:e.pia,e.side,e.value);
        }
        uint64_t before=instructions;
        if(devices) m68k_set_irq(board.irq());
        unsigned elapsed=m68k_execute(1); cycles+=elapsed;
        if(borrowedAddressRegister>=0){m68k_set_reg(m68k_register_t(M68K_REG_A0+borrowedAddressRegister),0);borrowedAddressRegister=-1;}
        if(devices) {board.tick(elapsed,shuffleWaiting && pc==pokeri::ShuffleWait::pc?0:elapsed);if(board.fault)stop(board.faultReason);}
        if(devices && board.resetRequested) {
            if(captures)fprintf(events,"watchdog CPU reset instruction=%llu cycles=%llu\n",instructions,cycles);
            if(captures)context(events);replay.event(4,instructions,cycles,relocation.canonical(m68k_get_reg(nullptr,M68K_REG_PC)));board.reset();cpuReset();
        }
        if(window.enabled && (pc==0x2472 || pc==0x246a))window.cabinetInput.observe(pc,board);
        if(preparing){
            startup.observe(pc);
            if(cycles>=nextSetupCycle){
                nextSetupCycle=cycles+uint64_t(hz/100);
                auto stage=startup.stage;
                startup.step(board,[&](unsigned pia,unsigned side,unsigned value){inputEvents.push_back({cycles,pia,side,value});});
                if(stage!=startup.stage && captures)fprintf(events,"setup stage=%u cycles=%llu reserve=%u\n",unsigned(startup.stage),cycles,startup.coins);
                if(startup.error)stop(startup.error);
            }
        }
        if(preparing && startup.stage==pokeri::Startup::Ready){
            cachePending=!startupCache.empty();
            startupTiming("original boot/operator setup complete");
            preparing=false;runStartCycles=cycles;runStartInstructions=instructions;
            window.ready(cycles);if(liveAudio){window.openAudio();output.window=&window;}
            puts(accountingLoaded?"Ready. Retained accounting restored; C: coin; Space: deal/draw; Esc: quit.":"Ready. Zero credits; C: coin; Space: deal/draw; Esc: quit.");
        }
#ifdef POKERI_HOST_ACRTC_TIMING
        if(acrtcDoubleScenario && !preparing && startup.stage==pokeri::Startup::Ready){
            board.inputRead=acrtcInputRead;
            bool wasDouble=acrtcPlayer.player.doubled;
            acrtcPlayer.step(board,cycles-runStartCycles,[&](unsigned code,bool down){
                fprintf(events,"scenario-key cycle=%llu round=%u code=%x down=%u\n",cycles,acrtcPlayer.player.round,code,unsigned(down));
            });
            if(!wasDouble && acrtcPlayer.player.doubled)fprintf(events,"scenario-double cycle=%llu round=%u\n",cycles,acrtcPlayer.player.round);
            if(acrtcPlayer.player.doubled && !acrtcDoubleAccepted && !memory[0x4112f]){
                acrtcDoubleAccepted=true;fprintf(events,"scenario-double-accepted cycle=%llu\n",cycles);
            }
            if(acrtcPlayer.failed || acrtcPlayer.player.failed)stop("Double timing scenario failed");
            if(acrtcPlayer.player.done){if(!acrtcDoubleAccepted)stop("Double key was not accepted by the ROM");fprintf(events,"scenario-done cycle=%llu round=%u double_ready=%u\n",cycles,acrtcPlayer.player.round,unsigned(memory[0x4112f]));userQuit=true;}
        }
#endif
        if(cycles>=nextFrame) {
            frameNumber=uint64_t((long double)cycles*frameHz/hz);nextFrame=uint64_t((long double)(frameNumber+1)*hz/frameHz);
            if(frameEvery && frameNumber%frameEvery==0) {char suffix[64];snprintf(suffix,sizeof suffix,"-frame-%06llu.ppm",frameNumber);writeFrame(out+suffix,presentationFrame());}
            if(window.enabled){if(!window.poll(board,!preparing))userQuit=true;window.show(presentationFrame(),cycles,board.config.cpuHz,!preparing);}
            if(shuffleEnabled && !shuffleProducer && shuffleQueue.held){
                captureShuffleFrame();
                shuffleQueue.release();board.video.presentationBusy=false;++shuffleSteps;
                if(captures)fprintf(events,"shuffle boundary=%llu cycles=%llu irqs=%llu\n",shuffleSteps,cycles,irqCount);
            }
        }
        if(before==instructions) {if(++inactive>1000) stop("CPU stopped without interrupt source");} else inactive=0;
        if(cachePending && !stopped){
            // Only untouched startup is cached, after its final frame bookkeeping.
            std::string temporary=startupCache+".new-"+std::to_string(getpid());
            try {
                snapshot(temporary,false,true);
                if(std::rename(temporary.c_str(),startupCache.c_str()))throw std::runtime_error("cannot publish startup cache");
            } catch(const std::exception &e){std::fprintf(stderr,"Startup cache unavailable: %s\n",e.what());std::remove(temporary.c_str());}
            cachePending=false;
        }
    }
    replay.event(5,instructions,cycles,relocation.canonical(m68k_get_reg(nullptr,M68K_REG_PC)),irqCount,stopped?1:0);replay.close();
    window.finishAudio();
    if(audio)wav.close();
    if(captures){
    {FILE*f=openfile(out+"-low-accesses.csv","w");fprintf(f,"pc,address,size,direction\n");
     for(auto &a:lowAccesses)fprintf(f,"%06x,%06x,%u,%c\n",std::get<0>(a),std::get<1>(a),std::get<2>(a),std::get<3>(a));fclose(f);}
    {FILE*f=openfile(out+"-rom-writes.csv","w");fprintf(f,"pc,address,size,direction\n");
     for(auto &a:romWrites)fprintf(f,"%06x,%06x,%u,%c\n",std::get<0>(a),std::get<1>(a),std::get<2>(a),std::get<3>(a));fclose(f);}
    if(!ramWriters.empty()){
        FILE*f=openfile(out+"-ram-writers.csv","w");fprintf(f,"byte,pc,address,size,value,instruction\n");
        for(auto &entry:ramWriters){auto &w=entry.second;fprintf(f,"%06x,%06x,%06x,%u,%08x,%llu\n",entry.first,w.pc,w.address,w.size,w.value,w.instruction);}fclose(f);
    }
    }
    if(!saveState.empty()){if(!devices)throw std::runtime_error("state requires --devices");snapshot(saveState,false);}
    if(devices && (captures || captureFrames || frameEvery)) {
        auto frame=compose(board.video);writeFrame(out+"-final.ppm",frame);
        if(captures){FILE*f=openfile(out+"-indices.bin","wb");fwrite(frame.indices.data(),1,frame.indices.size(),f);fclose(f);
        f=openfile(out+"-vram.bin","wb");
        for(uint32_t a=0;a<=board.video.frameMask;++a){uint16_t word=board.video.readWord(a);fputc(word>>8,f);fputc(word&255,f);}fclose(f);}
    }
    if(!opcodeAuditPath.empty()){
        std::vector<std::pair<uint64_t,uint64_t>> rows(opcodeAudit.entries.begin(),opcodeAudit.entries.end());
        std::sort(rows.begin(),rows.end());FILE*f=openfile(opcodeAuditPath,"w");
        bool ok=fprintf(f,"pc,opcode,entries,valid68000,needs060isp\n")>=0;
        for(const auto &row:rows){unsigned word=row.first&0xffff;
            if(fprintf(f,"%05x,%04x,%llu,%u,%u\n",unsigned(row.first>>16),word,
                static_cast<unsigned long long>(row.second),m68k_is_valid_instruction(word,M68K_CPU_TYPE_68000),unsigned(OpcodeAudit::movep(word)))<0)ok=false;
        }
        if(fclose(f))ok=false;require(ok,"opcode audit write failed");
    }
    if(!pcHistogramPath.empty()){
        std::vector<std::pair<uint64_t,uint64_t>> rows(pcHistogram.begin(),pcHistogram.end());
        std::sort(rows.begin(),rows.end());
        FILE *f=openfile(pcHistogramPath,"w");fprintf(f,"board_second,pc,instructions\n");
        bool ok=true;
        for(const auto &row:rows)if(fprintf(f,"%llu,%05x,%llu\n",static_cast<unsigned long long>(row.first>>32),unsigned(row.first),static_cast<unsigned long long>(row.second))<0)ok=false;
        if(fclose(f))ok=false;require(ok,"PC histogram write failed");
    }
    if(captures){FILE *ram=openfile(out+"-ram.bin","wb");fwrite(memory.data()+0x40000,1,0x40000,ram);fclose(ram);}
    if(!accountingPath.empty() && !board.fault && startup.stage==pokeri::Startup::Ready){
        pokeri::RetainedAccounting image;image.encode(memory.data());FILE*f=openfile(accountingPath+".new","wb");
        bool okay=fwrite(image.bytes.data(),1,image.bytes.size(),f)==image.bytes.size();int closed=fclose(f);
        require(okay && !closed,"accounting write failed");require(!rename((accountingPath+".new").c_str(),accountingPath.c_str()),"accounting rename failed");
    }
    if(!retainedRam.empty() && !board.fault){FILE*f=openfile(retainedRam,"wb");require(fwrite(memory.data()+0x40000,1,0x40000,f)==0x40000,"retained RAM write failed");require(fclose(f)==0,"retained RAM close failed");}
    if(captures){
#ifndef POKERI_HOST_ACRTC_TIMING
    if(devices && !board.fault){
        pokeri::State state;board.state(state);
        FILE*f=openfile(out+"-board-state.bin","wb");size_t n=fwrite(state.bytes.data(),1,state.bytes.size(),f);int result=fclose(f);
        require(n==state.bytes.size() && !result,"device state output failed");
        std::vector<uint8_t> cpu(pokeri_cpu_state_size());pokeri_cpu_state(cpu.data(),0);
        f=openfile(out+"-cpu-state.bin","wb");n=fwrite(cpu.data(),1,cpu.size(),f);result=fclose(f);
        require(n==cpu.size() && !result,"CPU state output failed");
    }
#else
    fprintf(events,"timed FIFO words=%llu status_reads=%llu nonempty_reads=%llu full_reads=%llu busy_reads=%llu\n",
            (unsigned long long)timedVideo.wordsWritten,(unsigned long long)timedVideo.statusReads,
            (unsigned long long)timedVideo.notEmptyReads,(unsigned long long)timedVideo.fullReads,(unsigned long long)timedVideo.busyReads);
    fprintf(events,"timed estimates=%llu inferred_geometry=%llu raw_table_cycles=%llu table_hz=%llu\n",
            (unsigned long long)acrtcEstimatedCommands,(unsigned long long)acrtcInferredCommands,
            (unsigned long long)acrtcRawCycles,(unsigned long long)acrtcTableHz);
    fprintf(events,"timed ACRTC clock=%llu completed=%llu; snapshots omitted\n",
            (unsigned long long)timedVideo.clock(),(unsigned long long)timedVideo.completed());
#endif
    FILE*f=openfile(out+"-coverage.bin","wb");fwrite(coverage.data(),1,coverage.size(),f);fclose(f);
    f=openfile(out+"-context.txt","w");
    fprintf(f,"%s\n",stopped?reason.c_str():"budget");context(f);
    fclose(f);
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
    }
    if(shuffleEnabled)printf("Shuffle VBlank boundaries: %llu\n",shuffleSteps);
    if(videoCatalog)fclose(videoCatalog);
    if(trace)fclose(trace);if(events!=stderr)fclose(events);
    if(stopped && !captures){fprintf(stderr,"%s\n",reason.c_str());context(stderr);}
    if(captures)printf("%s: instructions=%llu cycles=%llu PC=%05x; captures %s-*\n",stopped?reason.c_str():"budget",instructions,cycles,m68k_get_reg(nullptr,M68K_REG_PC),out.c_str());
    return stopped?2:0;
} catch(const std::exception &e){fprintf(stderr,"%s\n",e.what());return 1;}
