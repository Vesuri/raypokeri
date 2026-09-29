// No ROM data: compare our linked assembly with newly assembled synthetic
// BTST / BEQ / MOVE.W instructions in the host-only reference CPU.
#include "musashi/m68k.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
static std::array<unsigned char,0x180000> memory;
static unsigned writes=0,written=0;
// Keep fixtures above every allocated native section; extraction checks this.
static const unsigned code=0x100000,source=0x110000,frame=0x120000,desc=0x130000,port=0x140000;
static unsigned read(unsigned a,unsigned n){assert(a+n<=memory.size());unsigned v=0;while(n--)v=(v<<8)|memory[a++];return v;}
static void write(unsigned a,unsigned n,unsigned v){assert(a+n<=memory.size());while(n){--n;memory[a+n]=v;v>>=8;}}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return read(a,1);}unsigned m68k_read_memory_16(unsigned a){return read(a,2);}unsigned m68k_read_memory_32(unsigned a){return read(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){write(a,1,v);}void m68k_write_memory_16(unsigned a,unsigned v){if(a==port+2){++writes;written=v;}write(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){write(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return read(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return read(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return read(a,4);}
void pokeri_exception(unsigned){assert(false && "unexpected CPU exception");}
}
int main(int argc,char **argv){
    assert(argc==3);std::ifstream meta(argv[2]);std::map<std::string,unsigned> s;std::string name;unsigned address;
    while(meta>>name>>address)s[name]=address;
    auto sym=[&](const char*n){assert(s.count(n));return s[n];};
    FILE*f=fopen(argv[1],"rb");assert(f);
    auto word=[&](){unsigned n=0;for(int i=0;i<4;++i){int c=fgetc(f);assert(c>=0);n=n*256+c;}return n;};
    unsigned segments=word();while(segments--){unsigned a=word(),n=word();assert(a+n<code);assert(fread(memory.data()+a,1,n,f)==n);}fclose(f);
    m68k_init();unsigned checks=0;
    auto set=[&](const char*n,unsigned v,unsigned size=4){write(sym(n),size,v);};
    auto get=[&](const char*n){return read(sym(n),4);};
    auto run=[&](unsigned cpu,unsigned diagnostic,unsigned status,unsigned flags,unsigned value,unsigned stop,unsigned ptr,unsigned dueKind){
        bool valid=!(ptr&1) && ((ptr>=source && ptr<=source+0xffe) || (ptr>=code+0x100 && ptr<=code+0xffe));
        bool ready=status&2,branch=stop!=0,move=branch && stop!=1 && ready && valid;
        unsigned stepsExpected=1+unsigned(branch)+unsigned(move);
        // Independent synthetic branch displacement differs from the ROM.
        write(code,2,0x0810);write(code+2,2,1);write(code+4,2,0x6708);
        write(code+6,2,0x3159);write(code+8,2,2);write(code+14,2,0x4e71);
        write(port,1,status);if(valid)write(ptr,2,value);
        m68k_set_cpu_type(M68K_CPU_TYPE_68000);m68k_set_reg(M68K_REG_SR,0x2500|flags);m68k_set_reg(M68K_REG_SP,frame+0x2000);
        unsigned initial[15];for(unsigned r=0;r<15;++r){initial[r]=0x12345000+r;m68k_set_reg(m68k_register_t(M68K_REG_D0+r),initial[r]);}
        initial[8]=port;initial[9]=ptr;m68k_set_reg(M68K_REG_A0,port);m68k_set_reg(M68K_REG_A1,ptr);m68k_set_reg(M68K_REG_PC,code);
        writes=0;unsigned cycles=0;for(unsigned n=0;n<stepsExpected;++n)cycles+=m68k_execute(1);
        unsigned expectedPc=m68k_get_reg(nullptr,M68K_REG_PC),expectedSr=m68k_get_reg(nullptr,M68K_REG_SR),expectedA1=m68k_get_reg(nullptr,M68K_REG_A1);
        unsigned expectedWrites=writes,expectedValue=written;
        m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,frame);
        for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),initial[r]);
        m68k_set_reg(M68K_REG_PC,sym("nativeShortFeedRead"));m68k_set_reg(M68K_REG_A1,desc);
        for(unsigned i=0;i<4;++i)write(frame+i*4,4,initial[i<2?i:i+6]);
        write(frame+16,2,0x2500|flags);write(frame+18,4,code);write(frame+22,2,0x28);
        write(desc,4,code);write(desc+4,4,port);write(desc+8,2,2);write(desc+10,2,12);write(desc+28,4,desc+32);
        write(desc+32,4,code+6);write(desc+36,4,port+2);write(desc+40,2,0x0807);write(desc+42,2,16);
        write(desc+52,4,sym("nativeShortVideoWrite"));write(desc+56,2,4);
        set("nativeCachedVideoStatus",status,1);set("nativeDiagnostic",diagnostic,2);set("nativeFeedTarget",code+14);
        set("nativeRomBegin",code);set("nativeRomEnd",code+0x1000);set("nativeRamBegin",source);set("nativeRamEnd",source+0x1000);
        set("nativeShortPending",1,2);set("pendingFrames",0);set("seenFrames",0);
        set("nativeInstructions",1);set("nativeShortCalls",0);set("nativeShortNominal",12);
        for(auto n:{"nativeFeedTests","nativeFeedBranches","nativeFeedWrites"})set(n,0);
        writes=0;unsigned steps=0,pc=0,boundary=0;
        while((pc=m68k_get_reg(nullptr,M68K_REG_PC))!=sym("nativeShortLengthDone") && pc!=sym("nativeShortControlPromote") && steps++<150){
            if(pc==sym("nativeFeedBoundary0") || pc==sym("nativeFeedBoundary1")){
                boundary=pc==sym("nativeFeedBoundary0")?0:1;
                if(!diagnostic && stop==boundary){if(dueKind)set("nativeShortPending",2,2);else set("pendingFrames",1);}
            }
            if(pc==sym("nativeFeedReplayContinue") || pc==sym("nativeShortReplayStart") || pc==sym("nativeShortVideoWriteValue")){
                unsigned sp=m68k_get_reg(nullptr,M68K_REG_SP),result=1;
                if(pc==sym("nativeFeedReplayContinue"))result=stop!=boundary;
                else if(pc==sym("nativeShortReplayStart")){assert(read(sp+4,4)==code+6);set("nativeInstructions",get("nativeInstructions")+1);}
                else {assert(read(sp+4,4)==port+2 && read(sp+12,4)==7);written=read(sp+8,4);++writes;result=written;}
                // Every C-ABI scratch register is destroyed by the stub.
                m68k_set_reg(M68K_REG_D0,result);m68k_set_reg(M68K_REG_D1,0xdeadbeef);m68k_set_reg(M68K_REG_A0,0xabcdef00);m68k_set_reg(M68K_REG_A1,0x76543210);
                m68k_set_reg(M68K_REG_PC,read(sp,4));m68k_set_reg(M68K_REG_SP,sp+4);continue;
            }
            m68k_execute(1);
        }
        if(steps>=150 || read(frame+18,4)!=expectedPc || read(frame+16,2)!=expectedSr){fprintf(stderr,"feed mismatch cpu=%u diag=%u status=%x flags=%x stop=%u ptr=%x pc=%x/%x sr=%x/%x steps=%u\n",cpu,diagnostic,status,flags,stop,ptr,read(frame+18,4),expectedPc,read(frame+16,2),expectedSr,steps);abort();}
        assert(read(frame+12,4)==expectedA1 && writes==expectedWrites && (!writes || written==expectedValue));
        for(unsigned i=0;i<3;++i)assert(read(frame+i*4,4)==initial[i<2?i:8]);
        for(unsigned r=2;r<15;++r)if(r!=8 && r!=9)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==initial[r]);
        assert(m68k_get_reg(nullptr,M68K_REG_SP)==frame && read(frame+22,2)==0x28);
        assert(get("nativeInstructions")==1+(diagnostic?unsigned(move)+unsigned(branch):unsigned(move)*sym("nativeLiveCounterMode")));
        if(get("nativeShortNominal")!=unsigned(diagnostic?12:cycles)){fprintf(stderr,"cycles actual=%u expected=%u diag=%u status=%u stop=%u move=%u\n",get("nativeShortNominal"),cycles,diagnostic,status,stop,move);abort();}
        assert(get("nativeFeedTests")==sym("nativeFeedCounterMode") && get("nativeFeedBranches")==unsigned(branch)*sym("nativeFeedCounterMode") && get("nativeFeedWrites")==unsigned(move)*sym("nativeFeedCounterMode"));
        ++checks;
    };
    for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})for(unsigned diagnostic:{0u,1u})
    for(unsigned status=0;status<256;++status)for(unsigned flags=0;flags<32;++flags)
    for(unsigned value:{0u,1u,0x7fffu,0x8000u,0xffffu})for(unsigned stop:{0u,1u,2u})run(cpu,diagnostic,status,flags,value,stop,source,0);
    for(unsigned ptr:{0u,1u,code-2,code+0x100,code+0xffeu,code+0xfffu,code+0x1000,source-2,source+1,source+0xffeu,source+0xfffu,source+0x1000,0xfffffffEu,0xffffffffu})
    for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})for(unsigned diagnostic:{0u,1u})for(unsigned stop:{0u,1u,2u})
    for(unsigned status:{0u,2u})for(unsigned flags=0;flags<32;++flags)run(cpu,diagnostic,status,flags,0x8000,stop,ptr,1);
    printf("PASS: %u fused-feed cases: independent CPU sequence, every status/CCR, 68000/68020 execution, each intermediate boundary, exact nominal cycles, ABI clobbers and source guards\n",checks);
}
