// Newly constructed SUBQ/BNE oracle, not extracted program code. Its BNE is
// deliberately word-sized; adjust only the not-taken cycle and exit PC.
#include "musashi/m68k.h"
#include "native/DelayBudget.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <vector>
static std::array<unsigned char,65536> mem;
static unsigned read(unsigned a,unsigned n){assert(a+n<=mem.size());unsigned v=0;while(n--)v=v*256+mem[a++];return v;}
static void write(unsigned a,unsigned n,unsigned v){assert(a+n<=mem.size());while(n){--n;mem[a+n]=v;v>>=8;}}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return read(a,1);}unsigned m68k_read_memory_16(unsigned a){return read(a,2);}unsigned m68k_read_memory_32(unsigned a){return read(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){write(a,1,v);}void m68k_write_memory_16(unsigned a,unsigned v){write(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){write(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return read(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return read(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return read(a,4);}
void pokeri_exception(unsigned){assert(false && "unexpected exception");}
}
int main(int argc,char**argv){
 assert(argc==2);std::ifstream input(argv[1],std::ios::binary);assert(input);
 std::vector<char>kernel((std::istreambuf_iterator<char>(input)),{});assert(kernel.size()<1024);
 for(unsigned i=0;i<kernel.size();++i)mem[0x1000+i]=kernel[i];
 write(0x2000,2,0x5346);write(0x2002,2,0x6600);write(0x2004,2,0xfffc);write(0x2006,2,0x4e71);
 m68k_init();unsigned checks=0;
 auto test=[&](unsigned counter,unsigned steps){
  unsigned initial=0xa1230000|counter,cycles=0;
  m68k_set_cpu_type(M68K_CPU_TYPE_68000);m68k_set_reg(M68K_REG_SR,0x251f);m68k_set_reg(M68K_REG_SP,0x7000);m68k_set_reg(M68K_REG_PC,0x2000);m68k_set_reg(M68K_REG_D6,initial);
  for(unsigned n=0;n<steps;++n){unsigned pc=m68k_get_reg(nullptr,M68K_REG_PC);unsigned c=m68k_execute(1);if(pc==0x2002 && m68k_get_reg(nullptr,M68K_REG_PC)==0x2006)c-=4;cycles+=c;}
  unsigned expected=m68k_get_reg(nullptr,M68K_REG_D6),sr=m68k_get_reg(nullptr,M68K_REG_SR),pc=m68k_get_reg(nullptr,M68K_REG_PC);if(pc==0x2006)pc=0x2004;
  for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020}){
   for(unsigned i=0;i<16;++i)write(0x9000+4*i,4,i==6?initial:0x12340000+i);
   write(0x9040,4,0x2000);write(0x9044,2,0xa71f);
   m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,0x8000);m68k_set_reg(M68K_REG_PC,0x1000);
   for(unsigned r=2;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),0x34560000+r);
   write(0x8000,4,0x3000);write(0x8004,4,0x9000);write(0x8008,4,steps);
   unsigned n=0;while(m68k_get_reg(nullptr,M68K_REG_PC)!=0x3000 && ++n<100)m68k_execute(1);
   if(n>=100||read(0x9018,4)!=expected||read(0x9040,4)!=pc||read(0x9044,2)!=(0xa700|(sr&31))||m68k_get_reg(nullptr,M68K_REG_D0)!=cycles){
    fprintf(stderr,"delay mismatch counter=%u steps=%u cpu=%u D6=%x/%x pc=%x/%x sr=%x/%x cycles=%u/%u\n",counter,steps,cpu,read(0x9018,4),expected,read(0x9040,4),pc,read(0x9044,2),0xa700|(sr&31),m68k_get_reg(nullptr,M68K_REG_D0),cycles);abort();
   }
   for(unsigned r=0;r<16;++r)if(r!=6)assert(read(0x9000+r*4,4)==0x12340000+r);
   for(unsigned r=2;r<15;++r)if(r!=8&&r!=9)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==0x34560000+r);
   assert(m68k_get_reg(nullptr,M68K_REG_SP)==0x8004);++checks;
  }
 };
 for(unsigned counter=0;counter<65536;++counter)for(unsigned steps=1;steps<=4 && steps<=2*(counter?counter:65536);++steps)test(counter,steps);
 for(unsigned counter:{0u,1u,2u,3u,7424u,0x7fffu,0x8000u,0x8001u,0xffffu}){
  unsigned limit=2*(counter?counter:65536);
  for(unsigned steps:{limit,limit-1,limit/2,limit/2+1})if(steps)test(counter,steps);
 }
 unsigned budgets=0;
 for(unsigned available=0;available<=65522;++available)for(unsigned counter:{0u,1u,2u,3u,4680u,7424u,65535u}){
  unsigned steps=pokeri::delaySteps(counter,available),n=counter?counter:65536;
  assert(steps<=2*n);
  unsigned cycles=(steps/2)*14+(steps&1?4:0)-(steps && steps==2*n?2:0);
  assert(cycles<=available);if(available>=4)assert(steps);++budgets;
 }
 printf("PASS: %u 68000/68020 delay-state cases and %u cycle budgets; wrap, overflow, partial branches, exact cycles and all unrelated registers\n",checks,budgets);
}
