#include "../src/native/Hook.h"
#include "../src/native/PreparedHook.h"
#include "musashi/m68k.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>
using namespace pokeri;
struct Transaction {unsigned address,size,value;bool write;};
static std::array<unsigned char,65536> mem;
static std::vector<Transaction> transactions;
static unsigned rd(unsigned a,unsigned n){assert(a+n<=mem.size());unsigned v=0;for(unsigned i=0;i<n;++i)v=(v<<8)|mem[a+i];if(a>=0x2000)transactions.push_back({a,n,v,false});return v;}
static void wr(unsigned a,unsigned n,unsigned v){assert(a+n<=mem.size());for(unsigned i=0;i<n;++i)mem[a+i]=v>>(8*(n-i-1));if(a>=0x2000)transactions.push_back({a,n,v,true});}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return rd(a,1);} unsigned m68k_read_memory_16(unsigned a){return rd(a,2);} unsigned m68k_read_memory_32(unsigned a){return rd(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){wr(a,1,v);} void m68k_write_memory_16(unsigned a,unsigned v){wr(a,2,v);} void m68k_write_memory_32(unsigned a,unsigned v){wr(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return rd(a,1);} unsigned m68k_read_disassembler_16(unsigned a){return rd(a,2);} unsigned m68k_read_disassembler_32(unsigned a){return rd(a,4);}
void pokeri_exception(unsigned){}
}
struct Bus:HookBus {bool read(uint32_t a,unsigned n,uint32_t &v)override{v=rd(a,n);return true;}bool write(uint32_t a,unsigned n,uint32_t v)override{wr(a,n,v);return true;}};
static Operand op(Ea kind,int reg=-1,int ext=-1){return {kind,int8_t(reg),int8_t(ext)};}
static unsigned checks=0;
static bool preparedPath=false;
static void check(std::initializer_list<unsigned> words,Hook h,unsigned flags,unsigned seed){
    for(unsigned i=0;i<mem.size();++i)mem[i]=(i*13+seed)&255;
    unsigned pos=0x100;for(auto w:words){wr(pos,2,w);pos+=2;}
    auto original=mem;
    Registers r{};r.pc=0x100;r.sr=0x2700|flags;
    for(unsigned i=0;i<8;++i){r.d[i]=0x76543200+seed+i;r.a[i]=0x2000+i*0x100;}
    r.d[2]=seed&31;
    Registers initial=r;
    m68k_init();m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_set_reg(M68K_REG_SR,r.sr);m68k_set_reg(M68K_REG_PC,r.pc);
    for(unsigned i=0;i<8;++i){m68k_set_reg(m68k_register_t(M68K_REG_D0+i),r.d[i]);m68k_set_reg(m68k_register_t(M68K_REG_A0+i),r.a[i]);}
    transactions.clear();m68k_execute(1);auto expected=mem;auto accesses=transactions;
    mem=original;transactions.clear();Bus bus;PreparedHook prepared;assert(prepareHook(h,mem.data()+r.pc,prepared));assert(preparedPath?executePreparedHook(prepared,r,bus):executeHook(h,r,bus));
    bool okay=mem==expected && r.pc==m68k_get_reg(nullptr,M68K_REG_PC) && r.sr==m68k_get_reg(nullptr,M68K_REG_SR);
    for(unsigned i=0;i<8;++i)okay&=r.d[i]==m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+i)) && r.a[i]==m68k_get_reg(nullptr,m68k_register_t(M68K_REG_A0+i));
    okay&=accesses.size()==transactions.size();
    if(accesses.size()==transactions.size())for(unsigned i=0;i<accesses.size();++i){auto a=accesses[i],b=transactions[i];okay&=a.address==b.address && a.size==b.size && a.value==b.value && a.write==b.write;}
    // Failure at each data transaction must leave identical partial context
    // and memory; immutable instruction-extension fetches are not data faults.
    struct FailingBus:Bus {
        unsigned remaining;
        explicit FailingBus(unsigned n):remaining(n){}
        bool read(uint32_t a,unsigned n,uint32_t &v)override{if(a>=0x2000 && !remaining--)return false;return Bus::read(a,n,v);}
        bool write(uint32_t a,unsigned n,uint32_t v)override{if(a>=0x2000 && !remaining--)return false;return Bus::write(a,n,v);}
    };
    for(unsigned failure=0;failure<accesses.size();++failure){
        mem=original;Registers generic=initial,fast=initial;FailingBus gb(failure),fb(failure);
        assert(!executeHook(h,generic,gb));auto partial=mem;mem=original;
        assert(!executePreparedHook(prepared,fast,fb));assert(partial==mem);
        assert(generic.pc==fast.pc && generic.sr==fast.sr);
        for(unsigned i=0;i<8;++i)assert(generic.d[i]==fast.d[i] && generic.a[i]==fast.a[i]);
    }
    if(!okay){fprintf(stderr,"mismatch opcode %04x seed %u flags %u sr %04x expected %04x\n",*words.begin(),seed,flags,r.sr,m68k_get_reg(nullptr,M68K_REG_SR));assert(okay);}++checks;
}
static void clockProbe(std::initializer_list<unsigned> words,unsigned cycles){
    mem.fill(0);unsigned at=0x100;for(auto word:words){wr(at,2,word);at+=2;}
    m68k_init();m68k_set_cpu_type(M68K_CPU_TYPE_68000);m68k_set_reg(M68K_REG_SR,0x2700);
    m68k_set_reg(M68K_REG_PC,0x100);m68k_set_reg(M68K_REG_D0,8192);m68k_set_reg(M68K_REG_A0,0x2000);
    assert(m68k_execute(cycles)==int(cycles));assert(m68k_get_reg(nullptr,M68K_REG_PC)==at);
    assert((m68k_get_reg(nullptr,M68K_REG_D0)&65535)==0);
}
int main(){
    // Independent synthetic CPU probes, not game instructions or ROM data.
    clockProbe({0x5340,0x66fc},8192*14-2);
    clockProbe({0x1210,0x5340,0x66fa},8192*22-2);
    clockProbe({0xd481,0xb583,0x5340,0x66f8},8192*28-2);
    for(unsigned path=0;path<2;++path){preparedPath=path;
    for(unsigned flags=0;flags<32;++flags)for(unsigned seed: {0u,1u,127u,128u,255u}){
        check({0x117c,0x80,4},{0x100,6,1,Operation::move,op(Ea::immediate,-1,2),op(Ea::displacement,0,4)},flags,seed);
        check({0x12d8},{0x100,2,1,Operation::move,op(Ea::postincrement,0),op(Ea::postincrement,1)},flags,seed);
        check({0x10d8},{0x100,2,1,Operation::move,op(Ea::postincrement,0),op(Ea::postincrement,0)},flags,seed);
        check({0x1127},{0x100,2,1,Operation::move,op(Ea::predecrement,7),op(Ea::predecrement,0)},flags,seed);
        check({0x1230,0x2004},{0x100,4,1,Operation::move,op(Ea::indexed,0,2),op(Ea::data,1)},flags,seed);
        check({0x2238,0x2000},{0x100,4,4,Operation::move,op(Ea::absolute_word,-1,2),op(Ea::data,1)},flags,seed);
        check({0x2239,0,0x2000},{0x100,6,4,Operation::move,op(Ea::absolute_long,-1,2),op(Ea::data,1)},flags,seed);
        check({0x3250},{0x100,2,2,Operation::move,op(Ea::indirect,0),op(Ea::address,1)},flags,seed);
        check({0x4210},{0x100,2,1,Operation::clear,op(Ea::none),op(Ea::indirect,0)},flags,seed);
        check({0x4a50},{0x100,2,2,Operation::test,op(Ea::none),op(Ea::indirect,0)},flags,seed);
        check({0x0010,0x80},{0x100,4,1,Operation::or_bits,op(Ea::immediate,-1,2),op(Ea::indirect,0)},flags,seed);
        check({0x0210,0x80},{0x100,4,1,Operation::and_bits,op(Ea::immediate,-1,2),op(Ea::indirect,0)},flags,seed);
        check({0x0c10,0x80},{0x100,4,1,Operation::compare,op(Ea::immediate,-1,2),op(Ea::indirect,0)},flags,seed);
        check({0xb250},{0x100,2,2,Operation::compare,op(Ea::indirect,0),op(Ea::data,1)},flags,seed);
        check({0x0810,7},{0x100,4,1,Operation::bit_test,op(Ea::immediate,-1,2),op(Ea::indirect,0)},flags,seed);
        check({0x32fc,0x8000},{0x100,4,2,Operation::move,op(Ea::immediate,-1,2),op(Ea::postincrement,1)},flags,seed);
        check({0x22bc,0x8000,1},{0x100,6,4,Operation::move,op(Ea::immediate,-1,2),op(Ea::indirect,1)},flags,seed);
        check({0x137a,0x1efe,4},{0x100,6,1,Operation::move,op(Ea::pc_displacement,-1,2),op(Ea::displacement,1,4)},flags,seed);
        check({0x1368,4,8},{0x100,6,1,Operation::move,op(Ea::displacement,0,2),op(Ea::displacement,1,4)},flags,seed);
        check({0x0817,15},{0x100,4,1,Operation::bit_test,op(Ea::immediate,-1,2),op(Ea::indirect,7)},flags,seed);
        check({0x1ed8},{0x100,2,1,Operation::move,op(Ea::postincrement,0),op(Ea::postincrement,7)},flags,seed);
        check({0x0510},{0x100,2,1,Operation::bit_test,op(Ea::data,2),op(Ea::indirect,0)},flags,seed);
    }
    }
    printf("PASS %u native hook cases against Musashi: registers, CCR, memory and bus transactions\n",checks);
}
