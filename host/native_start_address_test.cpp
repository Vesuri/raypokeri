// Execute the user's original ROM routine and its dumped caller through the
// generated production hooks. ROM bytes and generated descriptors stay local.
#include "../amiga/generated/NativeTables.h"
#include "../src/native/PreparedHook.h"
#include "../src/board/Hd63484.h"
#include "musashi/m68k.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace pokeri;
static std::array<uint8_t,0x50000> memory;
static uint32_t base,device,activePc;
static bool native;
static Hd63484 video(false);
struct Transaction {uint32_t address,value;unsigned size;bool write;};
static std::vector<Transaction> transactions;
static unsigned transfer(unsigned a,unsigned n,unsigned v,bool writing){
    assert(a>=device && a+n<=device+4);
    if(native){
        bool allowed=false;
        for(const auto &e:accesses)if(e.pc==activePc && e.address==a-device+0xf6000 && e.size==n && e.write==writing)allowed=true;
        assert(allowed);
    }
    if(writing){for(unsigned i=0;i<n;++i)video.write8(a-device+i,v>>(8*(n-i-1)));}
    else {v=0;for(unsigned i=0;i<n;++i)v=(v<<8)|video.read8(a-device+i);}
    transactions.push_back({a-device+0xf6000,v,n,writing});return v;
}
static unsigned rd(unsigned a,unsigned n){
    if(!native && a>=device && a+n<=device+4)return transfer(a,n,0,false);
    if(!(a>=base && a-base+n<=memory.size())){fprintf(stderr,"unhooked/unmapped read %08x at %06x\n",a,activePc);std::abort();}
    unsigned v=0;for(unsigned i=0;i<n;++i)v=(v<<8)|memory[a-base+i];return v;
}
static void wr(unsigned a,unsigned n,unsigned v){
    if(!native && a>=device && a+n<=device+4){transfer(a,n,v,true);return;}
    if(!(a>=base && a-base+n<=memory.size())){fprintf(stderr,"unhooked/unmapped write %08x at %06x\n",a,activePc);std::abort();}
    for(unsigned i=0;i<n;++i)memory[a-base+i]=v>>(8*(n-i-1));
}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return rd(a,1);}unsigned m68k_read_memory_16(unsigned a){return rd(a,2);}unsigned m68k_read_memory_32(unsigned a){return rd(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){wr(a,1,v);}void m68k_write_memory_16(unsigned a,unsigned v){wr(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){wr(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return rd(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return rd(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return rd(a,4);}
void pokeri_exception(unsigned v){fprintf(stderr,"unexpected exception %u\n",v);std::abort();}
}
struct Bus {
    bool read(uint32_t a,unsigned n,uint32_t &v){v=transfer(a,n,0,false);return true;}
    bool write(uint32_t a,unsigned n,uint32_t v){transfer(a,n,v,true);return true;}
};
struct Result {Registers regs;std::array<uint8_t,256> control;std::vector<Transaction> bus;};
static std::array<uint8_t,0x40000> rom;
static Result run(unsigned placement,unsigned cpu,int offset,unsigned ccr,unsigned flags,bool caller){
    base=placement;native=base!=0;device=native?base+0x60000:0xf6000;
    memory.fill(0);std::copy(rom.begin(),rom.end(),memory.begin());video.control.fill(0);video.control[3]=ccr;video.ar=0;
    std::vector<PreparedHook> prepared(sizeof(hooks)/sizeof(*hooks));
    if(native){
        for(const auto &p:patchWords)assert(rd(base+p.offset,2)==p.value);
        for(const auto &f:fixups)wr(base+f.offset,4,rd(base+f.offset,4)+(f.kind==3?device-0xf6000:base));
        for(unsigned i=0;i<prepared.size();++i){assert(prepareHook(hooks[i],memory.data()+hooks[i].pc,prepared[i]));wr(base+hooks[i].pc,2,0xa000|i);}
    }
    // Authored jump stub at the address measured in the dump; its target is
    // the unmodified ROM routine. Start at the real caller's MOVEQ/JSR pair.
    wr(base+0x41fdc,2,0x4ef9);wr(base+0x41fde,4,base+0x29d2);
    wr(base+0x40600,4,base+0x1c528);
    m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2000|flags);
    for(unsigned i=0;i<8;++i){m68k_set_reg(m68k_register_t(M68K_REG_D0+i),0x87654321+i);m68k_set_reg(m68k_register_t(M68K_REG_A0+i),0x12345678+i);}
    m68k_set_reg(M68K_REG_D0,unsigned(offset));m68k_set_reg(M68K_REG_A6,base+0x48b00);m68k_set_reg(M68K_REG_SP,base+0x40600);
    m68k_set_reg(M68K_REG_PC,base+(caller?0x1c522:0x29d2));transactions.clear();
    unsigned hits=0,steps=0;
    while(m68k_get_reg(nullptr,M68K_REG_PC)!=base+0x1c528){
        assert(++steps<100);activePc=m68k_get_reg(nullptr,M68K_REG_PC)-base;
        unsigned opcode=rd(base+activePc,2);
        if(native && (opcode&0xf000)==0xa000){
            unsigned index=opcode&0xfff;assert(index<prepared.size() && hooks[index].pc==activePc);
            Registers r{};r.pc=base+activePc;r.sr=m68k_get_reg(nullptr,M68K_REG_SR);
            for(unsigned i=0;i<8;++i){r.d[i]=m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+i));r.a[i]=m68k_get_reg(nullptr,m68k_register_t(M68K_REG_A0+i));}
            Bus bus;assert(executePreparedHook(prepared[index],r,bus));
            for(unsigned i=0;i<8;++i){m68k_set_reg(m68k_register_t(M68K_REG_D0+i),r.d[i]);m68k_set_reg(m68k_register_t(M68K_REG_A0+i),r.a[i]);}
            m68k_set_reg(M68K_REG_SR,r.sr);m68k_set_reg(M68K_REG_PC,r.pc);++hits;
        }else m68k_execute(1);
    }
    assert(!video.error && transactions.size()==8 && (!native || hits==8));
    unsigned address=(0xb000-int16_t(caller?0:offset)*152)&0xfffff;
    unsigned actual=0;for(unsigned i=0xcc;i<0xd0;++i)actual=(actual<<8)|video.control[i];
    assert(actual==address && video.control[3]==ccr && video.ar==3);
    Result result{};result.control=video.control;result.bus=transactions;
    for(unsigned i=0;i<8;++i){result.regs.d[i]=m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+i));result.regs.a[i]=m68k_get_reg(nullptr,m68k_register_t(M68K_REG_A0+i));}
    result.regs.a[6]-=base;result.regs.a[7]-=base;result.regs.pc=m68k_get_reg(nullptr,M68K_REG_PC)-base;result.regs.sr=m68k_get_reg(nullptr,M68K_REG_SR);
    return result;
}
int main(){
    const char *names[]={"77POK30","77POK38","77POK34","PARA200J"};unsigned chip=0;
    for(const auto *name:names){char path[64];snprintf(path,sizeof(path),"rom/%s",name);FILE *f=fopen(path,"rb");assert(f);assert(fread(rom.data()+chip++*65536,1,65536,f)==65536 && fgetc(f)==EOF);fclose(f);}
    m68k_init();unsigned checks=0;
    for(bool caller:{false,true})for(int offset:{0,1,-1,291,32767,-32768})for(unsigned ccr:{0u,0x81u,0xffu})for(unsigned flags:{0u,0x1fu}){
        auto ref=run(0,M68K_CPU_TYPE_68000,offset,ccr,flags,caller);
        for(unsigned placement:{0x100000u,0x2ee3a00u}){
            auto got=run(placement,M68K_CPU_TYPE_68020,offset,ccr,flags,caller);
            assert(got.control==ref.control && got.regs.pc==ref.regs.pc && got.regs.sr==ref.regs.sr);
            for(unsigned i=0;i<8;++i)assert(got.regs.d[i]==ref.regs.d[i] && got.regs.a[i]==ref.regs.a[i]);
            for(unsigned i=0;i<ref.bus.size();++i){auto a=ref.bus[i],b=got.bus[i];assert(a.address==b.address && a.value==b.value && a.size==b.size && a.write==b.write);}
            ++checks;
        }
    }
    printf("PASS: %u original-ROM start-address/caller cases; eight guarded accesses, exact registers/CCR/device transactions, low and hardware-dump placements\n",checks);
}
