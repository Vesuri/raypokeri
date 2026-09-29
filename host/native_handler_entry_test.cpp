// Independent synthetic BTST/BNE/MOVE oracle; contains no game bytes or routine.
#include "musashi/m68k.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
static std::array<unsigned char,0x200000> mem;
static constexpr unsigned code=0x100000,frame=0x120000,desc=0x140000,port=0x180000,fields=0x190000;
static unsigned status,selectors[3];static bool oracle=false;
static unsigned rd(unsigned a,unsigned n){assert(a<=mem.size()-n);unsigned v=0;while(n--)v=v*256+mem[a++];return v;}
static void wr(unsigned a,unsigned n,unsigned v){assert(a<=mem.size()-n);while(n){--n;mem[a+n]=v;v>>=8;}}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return oracle && a==port?status:rd(a,1);}unsigned m68k_read_memory_16(unsigned a){return rd(a,2);}unsigned m68k_read_memory_32(unsigned a){return rd(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){if(oracle && a==port){selectors[0]=v;selectors[1]=selectors[2]=0;}else wr(a,1,v);}
void m68k_write_memory_16(unsigned a,unsigned v){wr(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){wr(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return rd(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return rd(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return rd(a,4);}
void pokeri_exception(unsigned v){std::fprintf(stderr,"unexpected exception %u\n",v);assert(false);}
}
int main(int argc,char **argv){
 assert(argc==3);std::map<std::string,unsigned>s;std::ifstream meta(argv[2]);std::string name;unsigned value;
 while(meta>>name>>value)s[name]=value;
 auto sym=[&](const char*n){assert(s.count(n));return s[n];};
 auto set=[&](const char*n,unsigned v,unsigned bytes=4){wr(sym(n),bytes,v);};
 std::ifstream in(argv[1],std::ios::binary);auto number=[&](){unsigned v=0;for(unsigned i=0;i<4;++i){int c=in.get();assert(c>=0);v=v*256+c;}return v;};
 unsigned segments=number();while(segments--){unsigned p=number(),n=number();assert(p+n<code);in.read((char*)mem.data()+p,n);assert(unsigned(in.gcount())==n);}
 // BTST #7,(A0); BNE.S error; MOVE.B #0,(A0).
 wr(code,2,0x0810);wr(code+2,2,7);wr(code+4,2,0x660e);wr(code+6,2,0x10bc);wr(code+8,2,0);
 const bool counts=s.count("nativeLiveCounterMode") && s["nativeLiveCounterMode"];
 m68k_init();unsigned cases=0;
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})for(status=0;status<256;++status)
 for(unsigned flags=0;flags<32;++flags)for(unsigned due=0;due<7;++due)for(unsigned bad=0;bad<2;++bad){
  unsigned initial[16];for(unsigned r=0;r<16;++r)initial[r]=0x76543000+r;
  initial[8]=port;initial[15]=0x160100;
  unsigned sr=0x2000|((flags&7)<<8)|flags;
  unsigned limit=(status&128)||bad?2:3,event=due?(due+1)/2:4;
  if(event<limit)limit=event;
  selectors[0]=0x91;selectors[1]=selectors[2]=1;
  oracle=true;m68k_set_cpu_type(M68K_CPU_TYPE_68000);m68k_set_reg(M68K_REG_SR,sr);
  for(unsigned r=0;r<16;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),initial[r]);
  m68k_set_reg(M68K_REG_PC,code);unsigned cycles=0;
  for(unsigned i=0;i<limit;++i)cycles+=m68k_execute(1);
  unsigned expected[16];for(unsigned r=0;r<16;++r)expected[r]=m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r));
  unsigned expectedPc=m68k_get_reg(nullptr,M68K_REG_PC),expectedSr=m68k_get_reg(nullptr,M68K_REG_SR);
  oracle=false;
  set("nativeDiagnostic",0,2);set("pendingFrames",0);set("seenFrames",0);set("nativeShortPending",0,2);
  set("nativeInstructions",counts?1:0);set("nativeShortNominal",12);set("nativeShortCalls",0);
  set("nativeFeedInlineCount",0);set("nativeFeedHeaderGrant",0);set("nativeProfileEnabled",flags&1,2);
  set("nativeShortDrainPc",code);set("nativeShortDrained",0);
  set("nativeClockResumePc",code);set("nativeCachedVideoStatus",status,1);wr(sym("nativeRegisters")+68,2,sr);
  set("nativeVideoSelector",fields);wr(sym("nativeVideoSelector")+4,4,fields+1);wr(sym("nativeVideoSelector")+8,4,fields+2);
  wr(fields,1,0x91);wr(fields+1,1,1);wr(fields+2,1,1);
  for(unsigned i=0;i<4;++i)wr(frame+i*4,4,initial[i<2?i:i+6]);
  wr(frame+16,2,0x0700|flags);wr(frame+18,4,code);wr(frame+22,2,0x28);
  wr(desc,4,code);wr(desc+28,4,desc+32);wr(desc+32,4,code+6);wr(desc+36,4,port+bad*2);
  wr(desc+40,2,0x0800);wr(desc+42,2,12);wr(desc+52,4,sym("nativeShortAddressWrite"));wr(desc+56,2,4);wr(desc+58,2,2);
  m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,frame);m68k_set_reg(M68K_REG_USP,initial[15]);
  for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),initial[r]);
  m68k_set_reg(M68K_REG_A1,desc);m68k_set_reg(M68K_REG_PC,sym("nativeShortHandlerEntry"));
  unsigned steps=0,pc;
  while((pc=m68k_get_reg(nullptr,M68K_REG_PC))!=sym("nativeShortControlPromote") && pc!=sym("nativeShortNoControlDue") && pc!=sym("nativeShortDecline") && steps++<500){
   unsigned boundary=pc==sym("nativeHandlerEntryTestBoundary")?1:pc==sym("nativeHandlerEntryBranchBoundary")?2:pc==sym("nativeShortLengthDone")?3:0;
   if(boundary==event){if(due&1)set("pendingFrames",1);else set("nativeShortPending",2,2);}
   m68k_execute(1);
  }
  if(steps>=500 || rd(frame+18,4)!=expectedPc || (rd(frame+16,2)&31)!=(expectedSr&31) || rd(sym("nativeShortNominal"),4)!=cycles){
   fprintf(stderr,"entry cpu=%u status=%u flags=%u due=%u bad=%u steps=%u pc=%x/%x sr=%x/%x cycles=%u/%u\n",cpu,status,flags,due,bad,steps,rd(frame+18,4),expectedPc,rd(frame+16,2),expectedSr,rd(sym("nativeShortNominal"),4),cycles);return 1;
  }
  assert(rd(sym("nativeInstructions"),4)==(counts?limit:0));
  for(unsigned i=0;i<3;++i)assert(rd(fields+i,1)==selectors[i]);
  for(unsigned i=0;i<4;++i)assert(rd(frame+i*4,4)==expected[i<2?i:i+6]);
  for(unsigned r=2;r<15;++r)if(r!=8 && r!=9)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==expected[r]);
  assert(rd(sym("nativeClockResumePc"),4)==expectedPc && rd(sym("nativeRegisters")+68,2)==sr);
  assert(m68k_get_reg(nullptr,M68K_REG_SP)==frame && m68k_get_reg(nullptr,M68K_REG_USP)==initial[15]);
  assert(rd(frame+22,2)==0x28 && (rd(frame+16,2)&0xffe0)==0x0700);
  assert(rd(sym("nativeShortDrained"),4)==unsigned(!(status&128) && (flags&1)));
  ++cases;
 }
 printf("PASS: %u linked entry cases; both CPUs, all status/CCR combinations, error branch, every event boundary and address fallback\n",cases);
}
