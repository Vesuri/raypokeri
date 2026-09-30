// CPU proof of the admission ABI, exact completed SUBQ/BNE state, late-frame
// promotion, and the already-paused fallback. Clock admission is stubbed here;
// its deadline policy is tested independently and by native runs.
#include "musashi/m68k.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
static std::array<unsigned char,0x1000000> mem;
static unsigned rd(unsigned a,unsigned n){assert(a+n<=mem.size());unsigned v=0;while(n--)v=v*256+mem[a++];return v;}
static void wr(unsigned a,unsigned n,unsigned v){assert(a+n<=mem.size());while(n){mem[a+--n]=v;v>>=8;}}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return rd(a,1);}unsigned m68k_read_memory_16(unsigned a){return rd(a,2);}unsigned m68k_read_memory_32(unsigned a){return rd(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){wr(a,1,v);}void m68k_write_memory_16(unsigned a,unsigned v){wr(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){wr(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return rd(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return rd(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return rd(a,4);}
void pokeri_exception(unsigned){assert(false && "unexpected CPU exception");}
}
int main(int argc,char**argv){
 assert(argc==3);std::map<std::string,unsigned>s;std::ifstream meta(argv[2]);std::string name;unsigned address;while(meta>>name>>address)s[name]=address;
 auto sym=[&](const char*n){assert(s.count(n));return s[n];};
 std::ifstream in(argv[1],std::ios::binary);auto num=[&](){unsigned v=0;for(unsigned i=0;i<4;++i){int c=in.get();assert(c>=0);v=v*256+c;}return v;};
 unsigned segments=num();while(segments--){unsigned a=num(),n=num();assert(a+n<0xf0000);in.read((char*)mem.data()+a,n);assert(unsigned(in.gcount())==n);}
 const unsigned sp=0x180000,base=0x200000,pc=base+0x2442,usp=0x190000;
 unsigned cases=0;m68k_init();
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})
 for(unsigned counter:{1u,2u,3u,571u,572u,0x7fffu,0x8000u,0xffffu,0u})
 for(unsigned flags=0;flags<32;++flags)
 for(unsigned mode=0;mode<6;++mode){
  // mode 0 success, 1 helper refusal, 2 diagnostic, 3 wrong site,
  // 4 wrong opcode index, 5 a VBI arriving after the helper's last check.
  unsigned regs[15];for(unsigned i=0;i<15;++i)regs[i]=0x12340000+i;
  regs[6]=0xabcd0000|counter;
  m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);
  for(unsigned i=0;i<15;++i)m68k_set_reg(m68k_register_t(M68K_REG_D0+i),regs[i]);
  m68k_set_reg(M68K_REG_SP,sp);m68k_set_reg(M68K_REG_USP,usp);
  for(unsigned i=0;i<4;++i)wr(sp+4*i,4,regs[i<2?i:i+6]);
  wr(sp+16,2,flags);wr(sp+18,4,pc+(mode==3?2:0));wr(sp+22,2,0);
  m68k_set_reg(M68K_REG_D0,mode==4?0xffd:0xffc);m68k_set_reg(M68K_REG_D1,0xaabbccdd);
  m68k_set_reg(M68K_REG_A0,pc+(mode==3?2:0));m68k_set_reg(M68K_REG_A1,0xccbbaa00);
  wr(sym("nativeRomBegin"),4,base);wr(sym("nativeDiagnostic"),2,mode==2);
  wr(sym("pendingFrames"),4,77);wr(sym("seenFrames"),4,77);
  wr(sym("nativeClockRunning"),2,0);wr(sym("nativeClockResumePc"),4,0xdeadbeef);
  m68k_set_reg(M68K_REG_PC,sym("nativeStartupDelayGuard"));
  unsigned count=0,calls=0,stop=0;
  while(++count<150){
   unsigned p=m68k_get_reg(nullptr,M68K_REG_PC);
   if(p==pc+4 || p==sym("nativeShortDecline") || p==sym("nativeShortControlPromote") || p==sym("nativeSave")){stop=p;break;}
   if(p==sym("nativeTryStartupDelay")){
    ++calls;unsigned stack=m68k_get_reg(nullptr,M68K_REG_SP);
    assert(rd(stack+4,4)==regs[6] && rd(stack+8,4)==pc);
    m68k_set_reg(M68K_REG_D0,mode!=1);m68k_set_reg(M68K_REG_D1,0x76543210);
    m68k_set_reg(M68K_REG_A0,0xabcdef);m68k_set_reg(M68K_REG_A1,0xfedcba);
    m68k_set_reg(M68K_REG_PC,rd(stack,4));m68k_set_reg(M68K_REG_SP,stack+4);
    if(mode==5)wr(sym("pendingFrames"),4,78);
   }else m68k_execute(1);
  }
  assert(count<150);
  if(!counter || counter>571){
   assert(stop==sym("nativeShortDecline") && !calls && rd(sp+16,2)==flags && m68k_get_reg(nullptr,M68K_REG_D6)==regs[6]);
  }else if(mode==0){
   assert(stop==pc+4 && calls==1 && m68k_get_reg(nullptr,M68K_REG_SR)==4);
   assert(m68k_get_reg(nullptr,M68K_REG_SP)==usp);
   for(unsigned i=0;i<15;++i)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+i))==(i==6?regs[i]&0xffff0000:regs[i]));
   assert(rd(sym("nativeClockResumePc"),4)==pc+4 && rd(sym("nativeClockRunning"),2)==1 && rd(0xbfee01,1)==0x11);
  }else if(mode==1){
   assert(stop==sym("nativeSave") && calls==1 && m68k_get_reg(nullptr,M68K_REG_D0)==10);
   for(unsigned i=0;i<15;++i)assert(rd(sym("nativeRegisters")+i*4,4)==regs[i]);
   assert(m68k_get_reg(nullptr,M68K_REG_SP)==sp+16 && rd(sp+16,2)==flags && rd(sp+18,4)==pc);
  }else if(mode==5){
   assert(stop==sym("nativeShortControlPromote") && calls==1);
   assert(rd(sp+16,2)==4 && rd(sp+18,4)==pc+4 && m68k_get_reg(nullptr,M68K_REG_D6)==(regs[6]&0xffff0000));
  }else {assert(stop==sym("nativeShortDecline") && !calls && rd(sp+16,2)==flags && m68k_get_reg(nullptr,M68K_REG_D6)==regs[6]);}
  ++cases;
 }
 unsigned policy=0;
 const unsigned board=0x300000,stop=0x100000;
 const char* clearByte[]={"irqDiagnostic","irqQuit","irqLiveActive","_ZL9haveEvent"};
 const char* clearWord[]={"nativeClockEnabled","nativeShortPending","nativeClockCalibrating"};
 const char* clearLong[]={"irqLiveTicks","nativeFeedInlineCount","nativeFeedHeaderGrant","nativeRasterGrantActive"};
 const char* counters[]={"nativeStartupDelayShortHits","nativeIdleCalls","nativeIdleInstructions","nativeIdleCycles"};
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})
 for(unsigned counter:{0u,1u,2u,571u,572u,65535u})
 for(unsigned phase:{0u,1u,7u,7990u,7998u,7999u,8000u,0xffffffffu})
 for(unsigned reject=0;reject<16;++reject){
  for(auto n:clearByte)wr(sym(n),1,0);
  for(auto n:clearWord)wr(sym(n),2,0);
  for(auto n:clearLong)wr(sym(n),4,0);
  for(auto n:counters)wr(sym(n),4,100);
  wr(sym("irqStartupFast"),1,1);wr(sym("_ZL5board"),4,board);wr(board+sym("offset_fault"),1,0);
  wr(sym("irqGuestPhase"),4,phase);wr(sym("nativeRegisters")+68,2,0x201f);
  wr(sym("pendingFrames"),4,77);wr(sym("seenFrames"),4,77);wr(sym("nativeClockResumePc"),4,pc-2);
  switch(reject){
   case 1:wr(sym("irqStartupFast"),1,0);break;
   case 2:wr(sym("irqDiagnostic"),1,1);break;
   case 3:wr(sym("_ZL9haveEvent"),1,1);break;
   case 4:wr(board+sym("offset_fault"),1,1);break;
   case 5:wr(sym("irqQuit"),1,1);break;
   case 6:wr(sym("irqLiveActive"),1,1);break;
   case 7:wr(sym("irqLiveTicks"),4,1);break;
   case 8:wr(sym("nativeRegisters")+68,2,0x251f);break;
   case 9:wr(sym("nativeShortPending"),2,1);break;
   case 10:wr(sym("pendingFrames"),4,78);break;
   case 11:wr(sym("nativeClockCalibrating"),2,1);break;
   case 12:wr(sym("nativeFeedInlineCount"),4,1);break;
   case 13:wr(sym("nativeFeedHeaderGrant"),4,1);break;
   case 14:wr(sym("nativeRasterGrantActive"),4,1);break;
   case 15:break; // pending quantum raised by shared clock accounting below
  }
  m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,sp);
  for(unsigned i=2;i<15;++i)m68k_set_reg(m68k_register_t(M68K_REG_D0+i),0x76540000+i);
  wr(sp,4,stop);wr(sp+4,4,0xabcd0000|counter);wr(sp+8,4,pc);
  m68k_set_reg(M68K_REG_PC,sym("nativeTryStartupDelay"));unsigned steps=0,pauses=0;
  while(m68k_get_reg(nullptr,M68K_REG_PC)!=stop && ++steps<200){
   if(m68k_get_reg(nullptr,M68K_REG_PC)==sym("nativeClockPause")){
    ++pauses;unsigned stack=m68k_get_reg(nullptr,M68K_REG_SP);
    if(reject==15)wr(sym("irqLiveTicks"),4,1);
    m68k_set_reg(M68K_REG_PC,rd(stack,4));m68k_set_reg(M68K_REG_SP,stack+4);
   }else m68k_execute(1);
  }
  assert(steps<200 && pauses==1);
  unsigned cycles=counter*14-2;
  bool accept=!reject && counter && phase<8000 && cycles<8000-phase;
  assert(m68k_get_reg(nullptr,M68K_REG_D0)==unsigned(accept));
  assert(rd(sym("irqGuestPhase"),4)==phase+(accept?cycles:0));
  assert(rd(sym("irqLiveTicks"),4)==unsigned(reject==7 || reject==15));
  assert(rd(sym("nativeStartupDelayShortHits"),4)==100+accept && rd(sym("nativeIdleCalls"),4)==100+accept);
  assert(rd(sym("nativeIdleInstructions"),4)==100+(accept?counter*2:0));
  assert(rd(sym("nativeIdleCycles"),4)==100+(accept?cycles:0));
  for(unsigned i=2;i<15;++i)if(i!=8 && i!=9)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+i))==0x76540000+i);
  assert((m68k_get_reg(nullptr,M68K_REG_SR)&0xff00)==0x2700 && m68k_get_reg(nullptr,M68K_REG_SP)==sp+4);
  ++policy;
 }
 printf("PASS: %u 68000/68020 assembly admission, completed-delay CCR/register, fallback and late-frame cases; %u linked C deadline/admission cases\n",cases,policy);
}
