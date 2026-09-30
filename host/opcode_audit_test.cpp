#include "OpcodeAudit.h"
#include "musashi/m68k.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
static unsigned opcode;
extern "C" {
unsigned m68k_read_disassembler_8(unsigned a){return a==0?opcode>>8:a==1?opcode&255:0;}
unsigned m68k_read_disassembler_16(unsigned a){return (m68k_read_disassembler_8(a)<<8)|m68k_read_disassembler_8(a+1);}
unsigned m68k_read_disassembler_32(unsigned a){return (m68k_read_disassembler_16(a)<<16)|m68k_read_disassembler_16(a+2);}
}
int main(){
 unsigned found=0;
 for(opcode=0;opcode<65536;++opcode){
  char text[256];m68k_disassemble(text,0,M68K_CPU_TYPE_68000);
  bool reference=std::strncmp(text,"movep",5)==0;
  if(OpcodeAudit::movep(opcode)!=reference){std::fprintf(stderr,"classifier mismatch %04x: %s\n",opcode,text);return 1;}
  if(reference){++found;if(!m68k_is_valid_instruction(opcode,M68K_CPU_TYPE_68000))return 1;}
 }
 OpcodeAudit a;a.observe(0x40100,0x0108);a.observe(0x40100,0x0108);a.observe(0x40100,0x4e71);a.observe(0x100,0x0108);
 if(found!=256 || a.entries.size()!=3 || a.entries[(uint64_t(0x40100)<<16)|0x0108]!=2)return 1;
 std::puts("PASS opcode audit: exhaustive first-word MOVEP classification, per-PC counts and changed RAM instructions");
}
