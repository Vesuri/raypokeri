// Actual program entry, with ABI-clobbering constructor/main/finalizer doubles.
#include "musashi/m68k.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <vector>
static std::array<unsigned char,0x1000000> mem;
static std::map<std::string,unsigned> sym;
static unsigned read(unsigned a,unsigned n){assert(a+n<=mem.size());unsigned v=0;while(n--)v=v*256+mem[a++];return v;}
static void write(unsigned a,unsigned n,unsigned v){assert(a+n<=mem.size());assert(a<0xdff000 || a>0xdfffff);while(n){--n;mem[a+n]=v;v>>=8;}}
extern "C" {
unsigned pokeri_service_cpu_type(void);
unsigned m68k_read_memory_8(unsigned a){return read(a,1);}unsigned m68k_read_memory_16(unsigned a){return read(a,2);}unsigned m68k_read_memory_32(unsigned a){return read(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){write(a,1,v);}void m68k_write_memory_16(unsigned a,unsigned v){write(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){write(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return read(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return read(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return read(a,4);}
void pokeri_exception(unsigned v){std::fprintf(stderr,"Unexpected CPU exception %u\n",v);std::exit(1);}
}
int main(int argc,char**argv){
 assert(argc==3);std::ifstream image(argv[1],std::ios::binary),symbols(argv[2]);assert(image&&symbols);
 image.read((char*)mem.data(),0x100000);assert(image.gcount()>0);
 std::string name;unsigned address;while(symbols>>name>>std::hex>>address)sym[name]=address;
 std::vector<unsigned> calls;
 for(auto prefix:{"__preinit_array_","__init_array_"})
  for(unsigned p=sym.at(std::string(prefix)+"start");p<sym.at(std::string(prefix)+"end");p+=4)calls.push_back(read(p,4));
 const unsigned mainIndex=calls.size();calls.push_back(sym.at("main"));
 for(unsigned p=sym.at("__fini_array_end");p>sym.at("__fini_array_start");p-=4)calls.push_back(read(p-4,4));
 assert(calls.size()>mainIndex+1); // actual game has a finalizer that may clobber D0
 m68k_init();unsigned cases=0;
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020,M68K_CPU_TYPE_68030,M68K_CPU_TYPE_68040}){
  m68k_set_cpu_type(cpu);assert(pokeri_service_cpu_type()==cpu);
  for(unsigned value:{0u,1u,20u,21u,0x7fffffffu,0x80000000u,0xffffffffu}){
   m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,0x110000);m68k_set_reg(M68K_REG_PC,sym.at("_start"));
   for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),0x12340000+r);
   write(0x110000,4,0x120000);unsigned index=0,steps=0;
   while(m68k_get_reg(nullptr,M68K_REG_PC)!=0x120000 && ++steps<200){
    unsigned pc=m68k_get_reg(nullptr,M68K_REG_PC);
    if(index<calls.size() && pc==calls[index]){
     unsigned sp=m68k_get_reg(nullptr,M68K_REG_SP);
     if(index==mainIndex)assert(read(sp+4,4)==0 && read(sp+8,4)==0);
     m68k_set_reg(M68K_REG_PC,read(sp,4));m68k_set_reg(M68K_REG_SP,sp+4);
     for(unsigned r:{0u,1u,8u,9u})m68k_set_reg(m68k_register_t(M68K_REG_D0+r),0xbad00000+r);
     if(index==mainIndex)m68k_set_reg(M68K_REG_D0,value);
     ++index;
    }else m68k_execute(1);
   }
   assert(steps<200 && index==calls.size());
   assert(m68k_get_reg(nullptr,M68K_REG_D0)==value && m68k_get_reg(nullptr,M68K_REG_SP)==0x110004);
   for(unsigned r=2;r<15;++r)if(r!=8&&r!=9)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==0x12340000+r);
   ++cases;
  }
 }
 printf("PASS: %u linked startup cases, constructor/finalizer order, ABI clobbers, zero argc/argv and full 32-bit main status preserved on 000/020/030/040\n",cases);
}
