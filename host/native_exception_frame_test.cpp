// Synthetic frame/vector data only; tests the linked native assembly on both CPUs.
#include "musashi/m68k.h"
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
 std::vector<char>kernel((std::istreambuf_iterator<char>(input)),{});assert(kernel.size()<128);
 for(unsigned i=0;i<kernel.size();++i)mem[0x1000+i]=kernel[i];
 m68k_init();unsigned checks=0;
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020}){
  m68k_set_cpu_type(cpu);
  for(unsigned sr=0;sr<65536;++sr){
   unsigned frame=0x4000+(sr&15),vector=0x5000+((sr>>4)&15);
   unsigned pc=0x9e3779b9u*sr,target=~pc;
   for(unsigned n=0;n<32;++n)mem[0x3ffc+n]=0xa5;
   write(vector,4,target);
   m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,0x8000);m68k_set_reg(M68K_REG_PC,0x1000);
   for(unsigned r=2;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),0x34560000+r);
   write(0x8000,4,0x3000);write(0x8004,4,frame);write(0x8008,4,0xbeef0000|sr);
   write(0x800c,4,pc);write(0x8010,4,vector);
   unsigned n=0;while(m68k_get_reg(nullptr,M68K_REG_PC)!=0x3000 && ++n<40)m68k_execute(1);
   assert(n<40 && read(frame,2)==sr && read(frame+2,4)==pc);
   assert(read(vector,4)==target && m68k_get_reg(nullptr,M68K_REG_D0)==target);
   for(unsigned a=0x3ffc;a<0x401c;++a)if(a<frame||a>=frame+6)assert(mem[a]==0xa5);
   for(unsigned r=2;r<15;++r)if(r!=8&&r!=9)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==0x34560000+r);
   assert(m68k_get_reg(nullptr,M68K_REG_SP)==0x8004);++checks;
  }
 }
 printf("PASS: %u linked 68000/68020 frame cases; all SR values, even/odd alignments, exact six bytes, vector and C ABI preservation\n",checks);
}
