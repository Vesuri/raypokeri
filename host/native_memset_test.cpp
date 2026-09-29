// Executes native assembly, checking exact byte effects, bounds and the C ABI.
#include "musashi/m68k.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
static std::array<uint8_t,0x400000> memory;
static unsigned first,last,writes;
static bool watch=false;
static unsigned read(unsigned a,unsigned n){assert(a+n<=memory.size());assert(n==1||!(a&1));unsigned v=0;while(n--)v=(v<<8)|memory[a++];return v;}
static void write(unsigned a,unsigned n,unsigned v){assert(a+n<=memory.size());assert(n==1||!(a&1));if(watch){assert(a>=first&&a+n<=last);writes+=n;}while(n--){memory[a+n]=v;v>>=8;}}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return read(a,1);}unsigned m68k_read_memory_16(unsigned a){return read(a,2);}unsigned m68k_read_memory_32(unsigned a){return read(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){write(a,1,v);}void m68k_write_memory_16(unsigned a,unsigned v){write(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){write(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return read(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return read(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return read(a,4);}
void pokeri_exception(unsigned){assert(false&&"unexpected exception in memset");}
}
int main(int argc,char **argv){
 assert(argc==2);std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> code((std::istreambuf_iterator<char>(f)),{});assert(!code.empty()&&code.size()<4096);
 const unsigned entry=0x1000,stop=0xf0000,sp=0x300000;
 std::copy(code.begin(),code.end(),memory.begin()+entry);unsigned cases=0;
 m68k_init();
 auto run=[&](unsigned address,unsigned size,unsigned value){
  first=address;last=address+size;writes=0;
  if(size)std::fill(memory.begin()+address-16,memory.begin()+last+16,uint8_t(value^0x5a));
  unsigned regs[16];for(unsigned i=0;i<16;++i)regs[i]=0x13579000u+i*0x1234;regs[15]=sp;
  m68k_set_reg(M68K_REG_SR,0x271f);for(unsigned i=0;i<16;++i)m68k_set_reg(m68k_register_t(M68K_REG_D0+i),regs[i]);
  write(sp,4,stop);write(sp+4,4,address);write(sp+8,4,value);write(sp+12,4,size);m68k_set_reg(M68K_REG_PC,entry);
  unsigned steps=0;watch=true;
  while(m68k_get_reg(nullptr,M68K_REG_PC)!=stop && ++steps<size*4+200)m68k_execute(1);
  watch=false;assert(steps<size*4+200&&writes==size);
  assert(m68k_get_reg(nullptr,M68K_REG_D0)==address&&m68k_get_reg(nullptr,M68K_REG_SP)==sp+4);
  for(unsigned i=2;i<15;++i)if(i!=8&&i!=9)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+i))==regs[i]);
  if(size){for(unsigned i=address;i<last;++i)assert(memory[i]==uint8_t(value));for(unsigned i=1;i<=16;++i){assert(memory[address-i]==uint8_t(value^0x5a));assert(memory[last+i-1]==uint8_t(value^0x5a));}}
  ++cases;
 };
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020}){
  m68k_set_cpu_type(cpu);run(0,0,0);
  for(unsigned offset=0;offset<8;++offset)for(unsigned n=0;n<=512;++n)for(unsigned value:{0u,1u,0x55u,0xffu,0x12345678u,0xffffffffu})run(0x100020+offset,n,value);
  for(unsigned offset=0;offset<8;++offset)for(unsigned n:{1023u,4096u,65535u,65536u,65537u,262144u,524288u})for(unsigned value:{0u,0xabu,0xffffffffu})run(0x100020+offset,n,value);
 }
 std::printf("PASS %u native memset cases: 68000/020, odd/even alignments, 32-bit lengths, values, zero length, exact stores and preserved ABI\n",cases);
}
