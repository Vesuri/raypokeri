// Synchronous CPU faults must unwind without changing the host signal mask.
#include "musashi/m68k.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <csignal>
#include <cstdlib>
#undef assert
#define assert(test) do {if(!(test)){fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#test);std::exit(1);}}while(0)
static std::array<unsigned char,65536> memory;
static bool armed=false,writing=false;
static unsigned vectorSeen=0;
static unsigned get(unsigned a,unsigned n){assert(a+n<=memory.size());unsigned v=0;while(n--)v=v*256+memory[a++];return v;}
static void put(unsigned a,unsigned n,unsigned v){assert(a+n<=memory.size());while(n){--n;memory[a+n]=v;v>>=8;}}
extern "C" {
void pokeri_exception(unsigned vector){vectorSeen=vector;}
unsigned m68k_read_memory_8(unsigned a){return get(a,1);}
unsigned m68k_read_memory_16(unsigned a){if(armed && !writing && a==0x200){armed=false;m68k_pulse_bus_error();}return get(a,2);}
unsigned m68k_read_memory_32(unsigned a){return get(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){put(a,1,v);}
void m68k_write_memory_16(unsigned a,unsigned v){if(armed && writing && a==0x200){armed=false;m68k_pulse_bus_error();}put(a,2,v);}
void m68k_write_memory_32(unsigned a,unsigned v){put(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return get(a,1);}
unsigned m68k_read_disassembler_16(unsigned a){return get(a,2);}
unsigned m68k_read_disassembler_32(unsigned a){return get(a,4);}
}
int main(int argc,char**){
 m68k_init();unsigned checks=0;
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})for(bool write:{false,true})for(bool block:{false,true}){
  memory.fill(0);put(8,4,0x400);put(0x100,2,write?0x3080:0x3010);put(0x400,2,0x4e71);put(0x402,2,0x4e71);put(0x200,2,0xabcd);
  m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,0x8000);
  m68k_set_reg(M68K_REG_PC,0x100);m68k_set_reg(M68K_REG_A0,0x200);m68k_set_reg(M68K_REG_D0,0x12345678);
  sigset_t mask,prior,after;sigemptyset(&mask);sigaddset(&mask,SIGUSR1);
  assert(sigprocmask(block?SIG_BLOCK:SIG_UNBLOCK,&mask,&prior)==0);
  writing=write;armed=true;vectorSeen=0;unsigned cycles=m68k_execute(1);
  if(argc>1)printf("fault cpu=%u armed=%u vector=%u pc=%x cycles=%u\n",cpu,armed,vectorSeen,m68k_get_reg(nullptr,M68K_REG_PC),cycles);
  assert(!armed && vectorSeen==2 && m68k_get_reg(nullptr,M68K_REG_PC)==0x402);
  assert(m68k_get_reg(nullptr,M68K_REG_A0)==0x200 && m68k_get_reg(nullptr,M68K_REG_D0)==0x12345678);
  assert(get(0x200,2)==0xabcd && m68k_get_reg(nullptr,M68K_REG_SP)<0x8000);
  assert(sigprocmask(SIG_BLOCK,nullptr,&after)==0 && sigismember(&after,SIGUSR1)==int(block));
  assert(sigprocmask(SIG_SETMASK,&prior,nullptr)==0);
  unsigned hash=2166136261u;for(auto b:memory)hash=(hash^b)*16777619u;
  if(argc>1)printf("cpu=%u write=%u blocked=%u cycles=%u sp=%x ram=%x\n",cpu,write,block,cycles,m68k_get_reg(nullptr,M68K_REG_SP),hash);
  unsigned following=m68k_execute(1);assert(following>0 && m68k_get_reg(nullptr,M68K_REG_PC)==0x404);if(argc>1)printf("continue cycles=%u\n",following);++checks;
 }
 printf("PASS: %u synchronous bus faults, read/write, 68000/68020, vector/register/memory continuation and host signal masks\n",checks);
}
