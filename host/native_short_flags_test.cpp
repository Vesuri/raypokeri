// Execute the actual assembled sentinel body and compare its stacked CCR with
// Musashi executing independent synthetic CMP.L/TST.L instructions. No ROMs.
#include "musashi/m68k.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
static std::array<unsigned char,1048576> memory;
static unsigned read(unsigned a,unsigned n){assert(a+n<=memory.size());unsigned v=0;while(n--)v=(v<<8)|memory[a++];return v;}
static void write(unsigned a,unsigned n,unsigned v){assert(a+n<=memory.size());while(n){--n;memory[a+n]=v;v>>=8;}}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return read(a,1);}unsigned m68k_read_memory_16(unsigned a){return read(a,2);}unsigned m68k_read_memory_32(unsigned a){return read(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){write(a,1,v);}void m68k_write_memory_16(unsigned a,unsigned v){write(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){write(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return read(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return read(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return read(a,4);}
void pokeri_exception(unsigned){assert(false && "unexpected test CPU exception");}
}
int main(int argc,char **argv){
    assert(argc==8);FILE *file=fopen(argv[1],"rb");assert(file);
    unsigned length=fread(memory.data()+0x1000,1,256,file);assert(feof(file) && length && length<256);fclose(file);
    m68k_init();m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    const unsigned values[]={0,1,0x217e,0x40b00,0x7fffffff,0x80000000,0xfffffffe,0xffffffff};
    unsigned checks=0;
    for(unsigned type=0;type<4;++type)for(unsigned flags=0;flags<32;++flags)for(unsigned lhs:values)for(unsigned rhs:values){
        // Newly assembled synthetic forms: CMP.L 4(A2),D0; TST.L (A2);
        // CMP.L 8(A0),D4; TST.L (A0). These are not ROM excerpts.
        const unsigned op[]={0xb0aa,0x4a92,0xb8a8,0x4a90};
        write(0x200,2,op[type]);write(0x202,2,type==2?8:4);
        m68k_set_reg(M68K_REG_SR,0x2000|flags);m68k_set_reg(M68K_REG_SP,0x7000);
        m68k_set_reg(M68K_REG_PC,0x200);m68k_set_reg(M68K_REG_A0,0x6000);m68k_set_reg(M68K_REG_A2,0x6000);
        m68k_set_reg(M68K_REG_D0,lhs);m68k_set_reg(M68K_REG_D4,lhs);
        write(0x6000+(type&1?0:type==2?8:4),4,rhs);m68k_execute(1);
        unsigned expected=m68k_get_reg(nullptr,M68K_REG_SR)&31;
        m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,0x8000);
        m68k_set_reg(M68K_REG_PC,0x1000);m68k_set_reg(M68K_REG_A1,0x9000);
        m68k_set_reg(M68K_REG_D1,rhs);
        m68k_set_reg(M68K_REG_D0,0x12345678); // clock bookkeeping clobbered scratch D0
        write(0x8000,4,lhs);write(0x8010,2,0x2500|flags);write(0x8012,4,0x123400);
        write(0x9004,4,rhs);write(0x9008,2,0x8000|type);
        unsigned before[16];for(unsigned r=0;r<16;++r)before[r]=m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r));
        unsigned steps=0;while(m68k_get_reg(nullptr,M68K_REG_PC)!=0x1000+length && steps++<40)m68k_execute(1);
        assert(steps<40);assert(read(0x8010,2)==(0x2500|expected));assert(read(0x8012,4)==0x123400);
        for(unsigned r=1;r<16;++r)assert(before[r]==m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r)));
        ++checks;
    }
    printf("PASS: %u assembled sentinel cases: all CCR combinations, signed-overflow boundaries, unchanged nonscratch registers and frame PC\n",checks);
    file=fopen(argv[2],"rb");assert(file);length=fread(memory.data()+0x1000,1,512,file);assert(feof(file) && length && length<512);fclose(file);
    unsigned decline=0x1000+std::strtoul(argv[3],nullptr,10);
    const unsigned bounds[]={0x20000,0x24000,0x30000,0x34000};
    for(unsigned i=0;i<4;++i)write(std::strtoul(argv[4+i],nullptr,10),4,bounds[i]);
    const unsigned bases[]={0,1,2,4,8,0x1fff8,0x1fffc,0x20000,0x20002,0x20004,0x23ff4,0x23ff8,0x23ffc,0x23ffd,0x23ffe,0x24000,0x2fff8,0x2fffc,0x30000,0x33ff4,0x33ff8,0x33ffc,0x33ffe,0x34000,0x50000,0xfffffff8,0xfffffffc,0xffffffff};
    checks=0;
    for(unsigned type=0;type<4;++type)for(unsigned base:bases){
        unsigned ea=base+(type&1?0:type==2?8:4);
        bool owned=(ea>=bounds[0] && ea<=bounds[1]-4) || (ea>=bounds[2] && ea<=bounds[3]-4);
        bool accepted=base==0 || (!(ea&1) && owned);
        if(owned)write(ea,4,0x89abcdef);
        m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,0x8000);m68k_set_reg(M68K_REG_PC,0x1000);
        m68k_set_reg(M68K_REG_A0,0x123400);m68k_set_reg(M68K_REG_A1,0x9000);m68k_set_reg(M68K_REG_A2,base);
        m68k_set_reg(M68K_REG_D1,0xdeadbeef);
        write(0x8008,4,base);write(0x8012,4,0x123400);write(0x9004,4,0x12344321);write(0x9008,2,0x8000|type);
        unsigned steps=0,pc=0;
        while((pc=m68k_get_reg(nullptr,M68K_REG_PC))!=0x1000+length && pc!=decline && steps++<50)m68k_execute(1);
        assert(steps<50);assert((pc==0x1000+length)==accepted);
        if(accepted){assert(m68k_get_reg(nullptr,M68K_REG_D1)==(base?0x89abcdef:0x12344321));assert(m68k_get_reg(nullptr,M68K_REG_A0)==0x123400);}
        ++checks;
    }
    printf("PASS: %u assembled address guards: null vectors, ROM/RAM boundaries, odd pointers, unmapped space and wrapping addresses\n",checks);
}
