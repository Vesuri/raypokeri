#include <proto/exec.h>
#include <proto/dos.h>
#include <exec/memory.h>
#include "Native.h"
#include "board/Board.h"
#include "native/Hook.h"
#include "native/Replay.h"
#include "native/Sha256.h"
#include "../../../amiga/generated/NativeTables.h"
using namespace pokeri;
struct DosLibrary *DOSBase=nullptr;
extern "C" {
Registers nativeRegisters;
uint8_t nativeServiceStack[32768];
uint32_t nativeReturnStack,nativeOsUsp,nativePrepareStack,nativeOldLevel3;
void nativeLevel3();
[[noreturn]] void nativeAbort();
[[noreturn]] void nativePrepareAbort();
uint16_t nativePhysicalSr,nativePhysicalResume;
uint32_t nativeFastBoundary=0,nativeRomBegin=0,nativeRomEnd=0,nativeRamBegin=0,nativeRamEnd=0;
volatile uint32_t nativeStatus=0,nativeInstructions=0,nativeInterrupts=0,nativeLastPc=0,nativeCycles=0,nativeVectorsRestored=0;
const char *nativeError=nullptr;
void nativeEntry();void nativeLineA();void nativeTrace();void nativeFault();
#define TRAP(n) void nativeTrap##n();
TRAP(0) TRAP(1) TRAP(2) TRAP(3) TRAP(4) TRAP(5) TRAP(6) TRAP(7) TRAP(8) TRAP(9) TRAP(10) TRAP(11) TRAP(12) TRAP(13) TRAP(14) TRAP(15)
}
static Board *board;
static uint8_t *romAllocation,*rom,*guard,*replayData;
static uint32_t romBase,ramBase,guardBase,replaySize,virtualUsp,virtualSsp,lastGuardCycle,liveStopCycles,guardCursor;
static ReplayReader *reader;static ReplayEvent nextEvent;static bool haveEvent,diagnostic=true;
static uint32_t savedVectors[48];
static_assert(offsetof(Registers,a)==32 && offsetof(Registers,pc)==64 && offsetof(Registers,sr)==68,"assembly register layout");static volatile uint32_t pendingFrames=0;static volatile bool installed=false,quitRequested=false;
static uint16_t originalControl[sizeof(controls)/sizeof(*controls)];
static uint32_t get32(const uint8_t*p){return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];}
static uint16_t get16(const uint8_t*p){return (uint16_t(p[0])<<8)|p[1];}
static void put32(uint8_t*p,uint32_t n){p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n;}
static void put16(uint8_t*p,unsigned n){p[0]=n>>8;p[1]=n;}
static bool fail(const char *s){if(!nativeError)nativeError=s;nativeStatus=0xdead;return false;}
extern "C" void pokeriRuntimeFault(const char *s){fail(s);if(installed)nativeAbort();nativePrepareAbort();}
static bool fileRead(const char *path,void *data,uint32_t size){BPTR f=Open(path,MODE_OLDFILE);if(!f)return fail("cannot open native input");LONG n=Read(f,data,size);uint8_t extra;LONG tail=Read(f,&extra,1);Close(f);return n==LONG(size) && tail==0?true:fail("native input size mismatch");}
static uint32_t canonical(uint32_t a){if(a>=romBase && a-romBase<0x40000)return a-romBase;if(a>=ramBase && a-ramBase<0x40000)return a-ramBase+0x40000;if(a>=guardBase && a-guardBase<0x80000)return a-guardBase+0x80000;return 0xffffffffu;}
static uint32_t relocated(uint32_t a){return a<0x40000?romBase+a:a<0x80000?ramBase+a-0x40000:guardBase+a-0x80000;}
static bool advanceEvent(){haveEvent=reader->next(nextEvent);nativeFastBoundary=diagnostic && haveEvent && !quitRequested?nextEvent.instruction:0;return haveEvent || reader->complete()?true:fail("invalid/truncated replay");}
static bool advanceClock(uint32_t target){if(target<nativeCycles)return fail("replay clock reversed");board->tick(target-nativeCycles);nativeCycles=target;return !board->fault || fail(board->faultReason);}
// Live service is bounded to 1 KB; diagnostic replay and exit inspect all 512 KB.
// One complete live sweep takes 512 serviced frames (10.24 s at 50 Hz).
static bool checkGuard(bool incremental=false){
    unsigned begin=incremental?guardCursor:0,end=incremental?begin+1024:0x80000;
    for(unsigned i=begin;i<end;i+=4)
        if(*(uint32_t*)(guard+i)!=0xa5a5a5a5)return fail("unhooked device write reached guard");
    if(incremental)guardCursor=end&0x7ffff;
    lastGuardCycle=nativeCycles;return true;
}
static void setSr(uint16_t value){value&=0xa71f;if(value&0x8000)fail("uncovered guest trace mode");Registers&r=nativeRegisters;if((r.sr^value)&0x2000){if(r.sr&0x2000){virtualSsp=r.a[7];r.a[7]=virtualUsp;}else{virtualUsp=r.a[7];r.a[7]=virtualSsp;}}r.sr=value;}
static bool pushException(unsigned vector,unsigned level){
    Registers&r=nativeRegisters;uint16_t sr=r.sr;setSr(uint16_t((sr|0x2000)&~0x8000));
    if(level)r.sr=uint16_t((r.sr&~0x700)|(level<<8));
    uint32_t sp=canonical(r.a[7]-6);if(sp<0x40000 || sp>=0x7fffa)return fail("virtual exception stack outside RAM");
    r.a[7]-=6;put16(board->memory.data()+sp,sr);put32(board->memory.data()+sp+2,r.pc);r.pc=get32(rom+vector*4);return true;
}
static void resetCpu(){setSr(0x2700);nativeRegisters.a[7]=get32(rom);nativeRegisters.pc=get32(rom+4);virtualSsp=nativeRegisters.a[7];}
struct Bus:HookBus {
    uint32_t pc;
    bool access(uint32_t a,unsigned size,bool writing,uint32_t &v){
        uint32_t local=canonical(a);
        // Five audited sentinel accesses observe immutable original vector data.
        if(a<32 && !writing && ((pc==0x616a && a==4)||(pc==0x6170 && a==0)||(pc==0x6186 && a==4)||(pc==0x61ca && a==0)||(pc==0x61e2 && a==8)) && size==4){v=get32(board->memory.data()+a);return true;}
        if(local==0xffffffffu || canonical(a+size-1)!=local+size-1)return fail("hook address outside allocation");
        if(local>=0x80000){
            unsigned first=0,last=sizeof(accesses)/sizeof(*accesses);
            while(first<last){unsigned middle=(first+last)/2;if(accesses[middle].pc<pc)first=middle+1;else last=middle;}
            bool allowed=false;
            while(first<sizeof(accesses)/sizeof(*accesses) && accesses[first].pc==pc){
                const auto &e=accesses[first++];
                if(e.address==local && e.size==size && e.write==writing){allowed=true;break;}
            }
            if(!allowed)return fail("device access outside hook table");
        }
        if(writing && local<0x40000){if(pc==0x2184 || pc==0x2358 || pc==0x25aa)return true;return fail("unexpected write to program image");}
        if(!writing)v=0;
        for(unsigned i=0;i<size;++i){if(writing){uint8_t b=v>>(8*(size-i-1));if(local<0x80000)board->memory[local+i]=b;else board->write8(local+i,b);}else v=(v<<8)|(local<0x40000?rom[local+i]:local<0x80000?board->memory[local+i]:board->read8(local+i));}
        return !board->fault || fail(board->faultReason);
    }
    bool read(uint32_t a,unsigned n,uint32_t&v)override{return access(a,n,false,v);}bool write(uint32_t a,unsigned n,uint32_t v)override{return access(a,n,true,v);}
};
static bool applyInput(const ReplayEvent &e){
    unsigned pia=e.a>>16,side=e.a&65535;
    if(pia>4 || (pia<3 && side>1) || (pia==3 && side!=0) || (pia==4 && (side>63 || (e.b>>16)>2)))return fail("invalid replay input");
    if(pia==4){std::vector<uint8_t> packet{uint8_t(side)};if((e.b>>16)==2)packet.push_back(e.b>>8);if(e.b>>16)packet.push_back(e.b);board->peer.enqueue(packet);}
    else if(pia==3)board->serial[side].receive.push_back(e.b);
    else board->pia[pia].input[side]=e.b;
    return true;
}
static bool liveInputs(){
    while(haveEvent){
        if(nextEvent.kind==ReplayInput){if(nextEvent.cycle>nativeCycles)break;if(!applyInput(nextEvent))return false;}
        if(!advanceEvent())return false;
    }
    return true;
}
static bool replayBoundary(){
    while(haveEvent && nextEvent.kind!=ReplayBus && nextEvent.kind!=ReplayPeripheralReset && nextEvent.instruction==nativeInstructions){
        ReplayEvent e=nextEvent;if(canonical(nativeRegisters.pc)!=e.pc)return fail("replay boundary PC mismatch");
        if(!advanceClock(e.cycle))return false;
        if(e.kind==ReplayIrq){if(board->irq()!=e.a || board->vector()!=e.b || ((nativeRegisters.sr>>8)&7)>=e.a)return fail("replay interrupt state mismatch");++nativeInterrupts;if(!pushException(e.b,e.a))return false;}
        else if(e.kind==ReplayReset){if(!board->resetRequested)return fail("replay watchdog not due");board->reset();resetCpu();}
        else if(e.kind==ReplayInput){if(!applyInput(e))return false;}
        else if(e.kind==ReplayEnd){nativeLastPc=canonical(nativeRegisters.pc);if(nativeInterrupts!=e.a)return fail("replay IRQ count mismatch");nativeStatus=2;advanceEvent();return false;}
        else return fail("unexpected replay event");
        if(!advanceEvent())return false;
    }
    if(haveEvent && nextEvent.instruction<nativeInstructions)return fail("missed replay boundary");
    return true;
}
extern "C" unsigned nativeDispatch(unsigned kind){
    if(quitRequested){nativeStatus=3;return false;}
    Registers&r=nativeRegisters;r.sr=uint16_t((r.sr&~31)|(nativePhysicalSr&31));uint32_t pc=canonical(r.pc);nativeLastPc=pc;
    if(kind==0)return fail("native CPU exception");
    if(pc>=0x80000)return fail("native PC outside ROM/RAM");
    ++nativeInstructions;
    if(kind==10){
        unsigned index=get16(rom+pc)&0xfff;
        if(index<sizeof(hooks)/sizeof(*hooks)){
            const pokeri::Hook &h=hooks[index];if(h.pc!=pc)return fail("Line-A index/site mismatch");
            bool device=hardwareHooks[index];
            if(diagnostic && device){if(!haveEvent || nextEvent.kind!=ReplayBus || nextEvent.instruction!=nativeInstructions || nextEvent.pc!=pc)return fail("replay I/O boundary mismatch");if(!advanceClock(nextEvent.cycle) || !advanceEvent())return false;}
            Bus bus;bus.pc=pc;if(!executeHook(h,r,bus))return fail("unsupported native hook");
        }else if(index==0xffe){r.d[7]=ramBase-0x40000;r.a[6]=0x40b00;r.pc+=6;}
        else if(index==0xffd){
            if(!(r.sr&0x2000))return fail("virtual privilege violation at RESET");
            bool found=false;for(auto offset:resets)if(pc==offset)found=true;if(!found)return fail("unknown RESET hook");
            if(diagnostic){if(!haveEvent || nextEvent.kind!=ReplayPeripheralReset || nextEvent.instruction!=nativeInstructions || nextEvent.pc!=pc)return fail("replay RESET mismatch");if(!advanceClock(nextEvent.cycle)||!advanceEvent())return false;}board->reset();r.pc+=2;
        }else if(index==0xffc){
            unsigned i=0;while(i<sizeof(controls)/sizeof(*controls) && controls[i]!=pc)++i;if(i==sizeof(controls)/sizeof(*controls))return fail("unknown CPU-control hook");uint16_t op=originalControl[i];
            if((op&0xfff8)!=0x40c0 && !(r.sr&0x2000))return fail("virtual privilege violation at CPU-control hook");
            if(op==0x4e73){uint32_t sp=canonical(r.a[7]);if(sp<0x40000 || sp>=0x7fffa)return fail("RTE stack outside RAM");uint16_t sr=get16(board->memory.data()+sp);r.pc=get32(board->memory.data()+sp+2);r.a[7]+=6;setSr(sr);}
            else if((op&0xfff0)==0x4e60){unsigned reg=op&7;if(op&8)r.a[reg]=virtualUsp;else virtualUsp=r.a[reg];r.pc+=2;}
            else if((op&0xfff8)==0x40c0){r.d[op&7]=(r.d[op&7]&0xffff0000)|r.sr;r.pc+=2;}
            else if(op==0x007c || op==0x027c || op==0x0a7c){unsigned operand=get16(rom+pc+2);setSr(op==0x007c?r.sr|operand:op==0x027c?r.sr&operand:r.sr^operand);r.pc+=4;}
            else return fail("unimplemented CPU-control form");
        }else return fail("unknown Line-A opcode");
    }else if(kind>=32 && kind<48){if(!pushException(kind,0))return false;}
    else if(kind!=9)return fail("unknown native exception vector");
    if(diagnostic){if(!replayBoundary())return false;}
    else {unsigned frames=pendingFrames;pendingFrames=0;while(frames--){if(!advanceClock(nativeCycles+160000)||!liveInputs())return false;}if(board->resetRequested){board->reset();resetCpu();}else if(board->irq()>((r.sr>>8)&7)){++nativeInterrupts;if(!pushException(board->vector(),board->irq()))return false;}}
    if(nativeStatus==0xdead)return false;
    if(!diagnostic && liveStopCycles && nativeCycles>=liveStopCycles){nativeLastPc=canonical(r.pc);nativeStatus=4;return false;}
    if(nativeCycles-lastGuardCycle>=160000 && !checkGuard(!diagnostic))return false;
    nativePhysicalResume=uint16_t((diagnostic?0x8000:0)|(r.sr&31));return true;
}
void nativeVbi(bool quit){++pendingFrames;if(quit){quitRequested=true;nativeFastBoundary=0;}}
extern "C" bool nativePrepareInner(){
    nativeStatus=0;DOSBase=(DosLibrary*)OpenLibrary("dos.library",0);if(!DOSBase)return fail("DOS unavailable");
    BPTR live=Open("native-live",MODE_OLDFILE);diagnostic=!live;
    if(live){uint8_t limit[5];LONG n=Read(live,limit,5);Close(live);if(n!=0 && n!=4)return fail("native-live must be empty or a four-byte cycle budget");if(n==4)liveStopCycles=get32(limit);}
    board=new Board();romAllocation=new uint8_t[0x40100];guard=new uint8_t[0x80000];if(!board||!romAllocation||!guard)return fail("native allocations failed");
    rom=(uint8_t*)((uint32_t(romAllocation)+255)&~255u);romBase=uint32_t(rom);ramBase=uint32_t(board->memory.data()+0x40000);guardBase=uint32_t(guard);
    nativeRomBegin=romBase;nativeRomEnd=romBase+0x40000;nativeRamBegin=ramBase;nativeRamEnd=ramBase+0x40000;
    static const char *names[]={"rom/77POK30","rom/77POK38","rom/77POK34","rom/PARA200J"};
    static const char *hashes[]={"2841c2393d469c744eb5e575b08f1e4f13320e73fd205eb27d2ea2cf4b59decd","fd87d156b71753d7ba03f548bf12bc3fee1d16477d1e89a9e9fe36521808ec8e","3facfb79dfd6942a197bc6f9456712cb1a0de92e0035a589711988148f07c0a7","ae1b91f898d8d69a36fde41bff1139c94c8b9cb93e77b4c697ddc43a362d244b"};
    for(unsigned chip=0;chip<4;++chip){uint8_t*d=board->memory.data()+(chip<<16);if(!fileRead(names[chip],d,65536))return false;uint8_t digest[32];sha256(d,65536,digest);for(unsigned i=0;i<64;++i){unsigned nibble=(digest[i>>1]>>(i&1?0:4))&15;if("0123456789abcdef"[nibble]!=hashes[chip][i])return fail("ROM SHA-256 mismatch");}}
    for(unsigned i=0;i<0x40000;++i)rom[i]=board->memory[i];
    for(unsigned i=0;i<0x80000;++i)guard[i]=0xa5;
    for(const auto &f:fixups){uint32_t v=get32(rom+f.offset);v+=f.kind==0?romBase:f.kind==3?guardBase-0x80000:ramBase-0x40000;put32(rom+f.offset,v);}
    put16(rom+0x10ae,0x6000);put16(rom+0x10b0,0x30);put16(rom+0x110c,0x6000);put16(rom+0x110e,0x2c);
    for(unsigned i=0;i<sizeof(hooks)/sizeof(*hooks);++i)put16(rom+hooks[i].pc,0xa000|i);
    for(auto pc:resets)put16(rom+pc,0xaffd);
    for(unsigned i=0;i<sizeof(controls)/sizeof(*controls);++i){originalControl[i]=get16(rom+controls[i]);put16(rom+controls[i],0xaffc);}
    put16(rom+0x2194,0xaffe);
    BPTR f=Open("replay.bin",MODE_OLDFILE);if(!f)return fail("replay.bin missing");Seek(f,0,OFFSET_END);LONG size=Seek(f,0,OFFSET_BEGINNING);if(size<9 || size>6000000){Close(f);return fail("replay size outside budget");}replaySize=size;replayData=new uint8_t[replaySize];if(!replayData){Close(f);return fail("replay allocation failed");}LONG got=Read(f,replayData,replaySize);Close(f);if(got!=size)return fail("replay read failed");reader=new ReplayReader(replayData,replaySize);if(!reader)return fail("replay reader allocation failed");
    uint32_t settings[10];for(unsigned i=0;i<10;++i){if(!reader->next(nextEvent) || nextEvent.kind!=ReplayConfig || nextEvent.pc!=i)return fail("replay config invalid");settings[i]=nextEvent.a;}
    const uint32_t supported[]={8000000,100,50,400,50000,1000000,0x3ffff,0,1,1};
    for(unsigned i=0;i<10;++i)if(settings[i]!=supported[i])return fail("unsupported native replay configuration");
    board->config.cpuHz=settings[0];board->config.systemHz=settings[1];board->config.inputHz=settings[2];board->config.watchdogMs=settings[3];board->config.watchdogResetUs=settings[4];board->ay.clockHz=settings[5];board->peer.enabled=settings[8];
    if(!advanceEvent())return false;
    resetCpu();if(diagnostic?!replayBoundary():!liveInputs())return false;nativePhysicalResume=diagnostic?0x8000:0;nativeStatus=1;return true;
}
extern "C" void nativeInstallVectors(){
    void(*traps[])()={nativeTrap0,nativeTrap1,nativeTrap2,nativeTrap3,nativeTrap4,nativeTrap5,nativeTrap6,nativeTrap7,nativeTrap8,nativeTrap9,nativeTrap10,nativeTrap11,nativeTrap12,nativeTrap13,nativeTrap14,nativeTrap15};
    volatile uint32_t *vectors=(volatile uint32_t*)0;
    nativeOldLevel3=vectors[27];vectors[27]=uint32_t(nativeLevel3);
    for(unsigned i=2;i<12;++i){savedVectors[i]=vectors[i];vectors[i]=uint32_t(i==9?nativeTrace:i==10?nativeLineA:nativeFault);}
    for(unsigned i=32;i<48;++i){savedVectors[i]=vectors[i];vectors[i]=uint32_t(traps[i-32]);}installed=true;
}
extern "C" void nativeRestoreVectors(){
    volatile uint32_t *vectors=(volatile uint32_t*)0;
    for(unsigned i=2;i<12;++i)vectors[i]=savedVectors[i];
    for(unsigned i=32;i<48;++i)vectors[i]=savedVectors[i];
    installed=false;
    vectors[27]=nativeOldLevel3;
    nativeVectorsRestored=vectors[27]==nativeOldLevel3;
    for(unsigned i=2;i<12;++i)if(vectors[i]!=savedVectors[i])nativeVectorsRestored=0;
    for(unsigned i=32;i<48;++i)if(vectors[i]!=savedVectors[i])nativeVectorsRestored=0;
}
extern "C" __attribute__((noinline)) void nativeReturned(){asm volatile("" ::: "memory");}
void nativeRun(){
    if(nativeStatus!=1)return;
    pendingFrames=0;quitRequested=false;
    Forbid();
    Supervisor((ULONG(*)())nativeEntry);
    Permit();
    checkGuard();
    if(!nativeVectorsRestored)fail("native vector restoration failed");
    nativeReturned();
}
void nativeRelease(){if(DOSBase && nativeError){PutStr(nativeError);PutStr("\n");}delete reader;delete[] replayData;delete[] guard;delete[] romAllocation;delete board;reader=nullptr;replayData=guard=romAllocation=nullptr;board=nullptr;if(DOSBase)CloseLibrary((Library*)DOSBase);DOSBase=nullptr;}
