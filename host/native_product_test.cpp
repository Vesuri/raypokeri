// Execute the cross-compiled product and compare with host wide arithmetic.
#include "musashi/m68k.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
static std::array<uint8_t,0x400000> memory;
static bool watch=false;
static const unsigned stack=0x300000,stop=0xf0000;
static unsigned read(unsigned a,unsigned n){assert(a+n<=memory.size());assert(n==1||!(a&1));unsigned v=0;while(n--)v=(v<<8)|memory[a++];return v;}
static void write(unsigned a,unsigned n,unsigned v){assert(a+n<=memory.size());assert(n==1||!(a&1));if(watch)assert(a>=stack-128&&a+n<=stack);while(n--){memory[a+n]=v;v>>=8;}}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return read(a,1);}unsigned m68k_read_memory_16(unsigned a){return read(a,2);}unsigned m68k_read_memory_32(unsigned a){return read(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){write(a,1,v);}void m68k_write_memory_16(unsigned a,unsigned v){write(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){write(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return read(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return read(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return read(a,4);}
void pokeri_exception(unsigned){assert(false&&"unexpected CPU exception");}
}
int main(int argc,char **argv){
 assert(argc==2);std::ifstream f(argv[1],std::ios::binary);auto num=[&](){unsigned v=0;for(unsigned i=0;i<4;++i){int c=f.get();assert(c>=0);v=v*256+c;}return v;};
 unsigned entry=num(),segments=num();while(segments--){unsigned a=num(),n=num();assert(a+n<stop);f.read((char*)memory.data()+a,n);assert(unsigned(f.gcount())==n);}
 unsigned cases=0,seed=0x379ead;auto random=[&](){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;};m68k_init();
 auto check=[&](uint32_t a,uint32_t b){
  unsigned regs[16];for(unsigned i=0;i<16;++i)regs[i]=0x12340000+i*0x1357;regs[15]=stack;
  m68k_set_reg(M68K_REG_SR,0x271f);for(unsigned i=0;i<16;++i)m68k_set_reg(m68k_register_t(M68K_REG_D0+i),regs[i]);
  write(stack,4,stop);write(stack+4,4,a);write(stack+8,4,b);m68k_set_reg(M68K_REG_PC,entry);unsigned steps=0;watch=true;
  while(m68k_get_reg(nullptr,M68K_REG_PC)!=stop&&++steps<512)m68k_execute(1);
  watch=false;assert(steps<512);
  uint64_t actual=(uint64_t(m68k_get_reg(nullptr,M68K_REG_D0))<<32)|m68k_get_reg(nullptr,M68K_REG_D1);
  assert(actual==uint64_t(a)*b&&m68k_get_reg(nullptr,M68K_REG_SP)==stack+4);
  for(unsigned i=2;i<15;++i)if(i!=8&&i!=9)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+i))==regs[i]);++cases;
 };
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020}){
  m68k_set_cpu_type(cpu);
  for(unsigned a:{0u,1u,50u,100u,1000u,8000u,65534u,65535u,65536u,65537u,80000u,1000000u,0x7fffffffu,0xffff0000u,0xffffffffu})
   for(unsigned b:{0u,1u,50u,100u,1000u,8000u,65534u,65535u,65536u,65537u,80000u,1000000u,0x7fffffffu,0xffff0000u,0xffffffffu})check(a,b);
  for(unsigned i=0;i<100000;++i){unsigned a=random(),b=random();if(i%3)b&=65535;check(a,b);}
 }
 std::printf("PASS %u linked 32x32-to-64 product cases: 68000/020, full-width rates, carries, ABI and stack bounds\n",cases);
}
