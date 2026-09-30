// Authored exception frames only. Exercise the production assembly bytes.
#include "musashi/m68k.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
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
static unsigned call(unsigned entry){
 m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,0x8000);m68k_set_reg(M68K_REG_PC,entry);
 for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),0x34560000+r);
 m68k_set_reg(M68K_REG_A0,0x4000);m68k_set_reg(M68K_REG_A1,0x5000);
 write(0x8000,4,0x3000);
 unsigned n=0;while(m68k_get_reg(nullptr,M68K_REG_PC)!=0x3000 && ++n<40)m68k_execute(1);
 assert(n<40 && m68k_get_reg(nullptr,M68K_REG_SP)==0x8004);
 for(unsigned r=1;r<15;++r)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==(r==8?0x4000:r==9?0x5000:0x34560000+r));
 return m68k_get_reg(nullptr,M68K_REG_D0);
}
int main(int argc,char**argv){
 assert(argc==3);std::ifstream input(argv[1],std::ios::binary);assert(input);
 std::vector<char>code((std::istreambuf_iterator<char>(input)),{});assert(code.size()<256);
 unsigned consume=0x1000+std::strtoul(argv[2],nullptr,0);
 for(unsigned i=0;i<code.size();++i)mem[0x1000+i]=code[i];
 m68k_init();unsigned cases=0;
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020}){
  m68k_set_cpu_type(cpu);
  for(unsigned sr=0;sr<65536;++sr)for(unsigned mode=0;mode<4;++mode){
   // Cover every SR and every extension byte without interpreting frame format.
   for(unsigned n=0;n<96;++n)mem[0x4000+n]=(sr+n*17)&255;
   write(0x4000,2,sr);write(0x4002,4,mode&1?0x6000:0x7200);
   write(0x5000,4,0x6000);write(0x5004,4,0x7300);write(0x5008,2,mode>>1);
   auto before=mem;unsigned expected=0;
   if(!(sr&0x2000)){
    if(mode==0)expected=1;
    else if(mode!=3)expected=~0u;
   }
   assert(call(0x1000)==expected);
   if(expected==1){before[0x5008]=0;before[0x5009]=1;
    for(unsigned n=0;n<4;++n){before[0x5004+n]=before[0x4002+n];before[0x4002+n]=before[0x5000+n];}}
   for(unsigned n=0x4000;n<0x6000;++n)assert(mem[n]==before[n]);
   // Back-to-back IRQ at the stub retains the first return PC.
   if(expected==1){assert(call(0x1000)==0);for(unsigned n=0x4000;n<0x6000;++n)assert(mem[n]==before[n]);}
   auto pending=mem;unsigned wanted=(!(sr&0x2000) && read(0x4002,4)==0x6000)?(read(0x5008,2)?1:~0u):0;
   assert(call(consume)==wanted);
   if(wanted==1){for(unsigned n=0;n<4;++n)pending[0x4002+n]=pending[0x5004+n];pending[0x5008]=pending[0x5009]=0;}
   for(unsigned n=0x4000;n<0x6000;++n)assert(mem[n]==pending[n]);
   ++cases;
  }
 }
 printf("PASS: %u assembled redirect/consume cases; all SRs, supervisor and back-to-back frames, stale/missing slots, untouched extensions/registers\n",cases);
}
