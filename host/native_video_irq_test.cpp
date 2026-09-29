// Synthetic states only; execute the real linked guard, clock charge and return.
#include "musashi/m68k.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
static std::array<uint8_t,0x300000> mem;
static std::vector<unsigned> stores;
static bool watching=false;
static unsigned timerWrites=0;
static unsigned rd(unsigned a,unsigned n){assert(a+n<=mem.size());unsigned v=0;while(n--)v=v*256+mem[a++];return v;}
static void wr(unsigned a,unsigned n,unsigned v){
 if(a==0xbfee01){assert(n==1 && v==0x11);++timerWrites;return;}
 assert(a+n<=mem.size());if(watching)for(unsigned i=0;i<n;++i)stores.push_back(a+i);
 while(n){--n;mem[a+n]=v;v>>=8;}
}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return rd(a,1);}unsigned m68k_read_memory_16(unsigned a){return rd(a,2);}unsigned m68k_read_memory_32(unsigned a){return rd(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){wr(a,1,v);}void m68k_write_memory_16(unsigned a,unsigned v){wr(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){wr(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return rd(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return rd(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return rd(a,4);}
void pokeri_exception(unsigned v){std::fprintf(stderr,"unexpected CPU exception %u\n",v);assert(false);}
}
enum Bad {Good,Diagnostic,Profile,NotReady,Stopped,WrongPc,Legacy,NoTimer,Calibration,NoOverhead,ScreenCalibration,PhysicalSupervisor,Budget,Startup,Trace,Ipl,OddStack,LowStack,EndStack,OddTarget,BadTarget,Pia,Serial,NoVideo,OtherVector,Fault,Reset,VideoError,Frame,Tick,ClockFrame,CreditDebt,Quit,Drain,Shuffle,ShuffleActive,ShuffleQueued,ShufflePointer,Hold,Display,PresentDue,CardDue,LateFrame,LateQuit,Format,LastBad};
int main(int argc,char**argv){
 assert(argc==3);std::map<std::string,unsigned>s;std::ifstream meta(argv[2]);std::string name;unsigned a;
 while(meta>>name>>a)s[name]=a;
 auto sym=[&](const std::string&n){assert(s.count(n));return s[n];};
 auto set=[&](const char*n,unsigned v,unsigned size=4){
  assert(size==sym(std::string("size_")+n) || (std::string(n)=="liveCycles" && size==4));wr(sym(n),size,v);
 };
 auto get=[&](const char*n,unsigned size=4){return rd(sym(n),size);};
 const unsigned board=0x100000,rom=board+sym("boardMemory"),ram=rom+0x40000,stack=0x2f0000,stop=0xf0000,card=0x1f0000;
 auto bf=[&](const char*n,unsigned v,unsigned size=1){wr(board+sym(n),size,v);};
 auto cf=[&](const char*n,unsigned v,unsigned size=4){wr(sym("liveClock")+sym(std::string("clock_")+n),size,v);};
 std::ifstream in(argv[1],std::ios::binary);auto num=[&](){unsigned v=0;for(unsigned i=0;i<4;++i){int c=in.get();assert(c>=0);v=v*256+c;}return v;};
 unsigned segments=num();while(segments--){unsigned p=num(),n=num();assert(p+n<stop);in.read((char*)mem.data()+p,n);assert(unsigned(in.gcount())==n);}
 std::vector<std::pair<unsigned,unsigned>> timeFields={{sym("liveClock"),24},{sym("guestClockPhase"),4},{sym("liveTicks"),4},{sym("nativeShortGuest"),4},{sym("nativeShortNominal"),4},{sym("nativeClockRunning"),2}};
 auto timeState=[&](){std::vector<uint8_t> v;for(auto f:timeFields)v.insert(v.end(),mem.begin()+f.first,mem.begin()+f.first+f.second);return v;};
 auto restoreTime=[&](const std::vector<uint8_t>&v){unsigned pos=0;for(auto f:timeFields)for(unsigned i=0;i<f.second;++i)mem[f.first+i]=v[pos++];};
 unsigned cases=0,admitted=0,races=0;stores.reserve(1024);m68k_init();
 unsigned sourceKind=0,sourceControl=0,sourceFlags=0;
 auto run=[&](unsigned cpu,unsigned flags,unsigned level,bool supervisor,Bad bad,bool wrapper,unsigned charge,unsigned edge){
  watching=false;stores.clear();timerWrites=0;m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);
  unsigned pc=rom+0x2ebc,usp=ram+0x1400,ssp=ram+0x2400,target=rom+0x600;
  if(edge==1)ssp=ram+6;if(edge==2)ssp=ram+0x3fffe;if(edge==3)target=ram+0x600;
  if(supervisor)usp=ssp;
  unsigned virtualSr=(supervisor?0x2000:0)|(level<<8)|((flags+7)&31),physical=flags;
  set("board",board);set("rom",rom);set("romBase",rom);set("ramBase",ram);
  set("nativeRomBegin",rom);set("nativeRomEnd",ram);set("nativeRamBegin",ram);set("nativeRamEnd",ram+0x40000);
  set("diagnostic",0,1);set("nativeDiagnostic",0,2);set("timingActive",0,1);set("nativeSetupReady",1);set("nativeStatus",1);
  set("nativeClockMode",2,2);set("nativeClockEnabled",1,2);set("nativeClockRunning",0,2);set("nativeClockOverhead",40);set("nativeClockCalibrating",0,2);
  set("clockDisplayCalibrated",1,1);set("startupFast",0,1);set("liveStopCycles",0);set("liveCycles",0);wr(sym("liveCycles")+4,4,64000000);
  set("liveTicks",0);set("guestClockPhase",1234);set("pendingFrames",10);set("seenFrames",10);
  cf("credit",0);cf("debt",480000);cf("frame",10);cf("discardedWall",0);cf("limited",0);cf("ratioSixteenths",64,2);cf("windowFrames",3,2);
  set("nativeShortGuest",charge==2?29:0);set("nativeShortNominal",charge?40:0);set("nativeShortPending",3,2);set("nativeShortDrained",0);
  set("nativeInterrupts",17);set("nativeInstructions",0xabcdef);set("nativeCycles",64000000);set("lastPresentCycle",64000000);
  set("quitRequested",0,1);set("shuffleActive",0,1);set("shuffleQueued",0,1);set("nativeShuffleNextPointer",0);
  wr(sym("shuffleQueue")+sym("shuffleCount"),4,0);wr(sym("screen")+sym("screenActive"),1,1);wr(sym("screen")+sym("screenPending"),4,0xffffffffu);
  set("nativeCardCache",card);wr(card+sym("cardHits"),4,7);set("presentedCardHits",7);
  set("liveIrqActive",1,1);set("uninterruptedPoll",1,1);set("nativeLastPc",0xb00);set("nativePhysicalSr",0x7777,2);set("nativePhysicalResume",0x5555,2);
  set("nativeCachedVideoStatus",0x23,1);set("nativeClockResumePc",pc);set("nativeExtendedFrame",cpu==M68K_CPU_TYPE_68020,2);
  if(s.count("nativeVideoIrqHits"))set("nativeVideoIrqHits",7);
  bf("boardFault",0);bf("boardReset",0);bf("videoError",0,4);bf("videoStatus",32);bf("videoHold",0);bf("videoEnable",0x81);bf("videoPending",0,4);bf("videoRead",0,4);
  bf("piacontrol0",0x36);bf("piaflags0",0x80);bf("piacontrol1",0x0e);bf("piaflags1",0);bf("serialControl",0x95);bf("serialRead",0,4);
  switch(bad){
   case Good:break;
   case Diagnostic:set("diagnostic",1,1);set("nativeDiagnostic",1,2);break;
   case Profile:set("timingActive",1,1);break;
   case NotReady:set("nativeSetupReady",0);break;case Stopped:set("nativeStatus",4);break;
   case WrongPc:pc+=2;break;case Legacy:set("nativeClockMode",1,2);break;case NoTimer:set("nativeClockEnabled",0,2);break;
   case Calibration:set("nativeClockCalibrating",1,2);break;case NoOverhead:set("nativeClockOverhead",0);break;
   case ScreenCalibration:set("clockDisplayCalibrated",0,1);break;case PhysicalSupervisor:physical|=0x2000;break;
   case Budget:set("liveStopCycles",64000000);break;case Startup:set("startupFast",1,1);break;
   case Trace:virtualSr|=0x8000;break;case Ipl:virtualSr=(virtualSr&~0x700)|0x500;break;
   case OddStack:if(supervisor)++usp;else ++ssp;break;
   case LowStack:if(supervisor)usp=ram+4;else ssp=ram+4;break;
   case EndStack:if(supervisor)usp=ram+0x40000;else ssp=ram+0x40000;break;
   case OddTarget:++target;break;case BadTarget:target=ram+0x40000;break;
   case Pia:bf("piacontrol0",1);break;case Serial:bf("serialRead",1,4);break;case NoVideo:bf("videoEnable",0);break;
   case OtherVector:bf("piacontrol1",0x28);bf("piaflags1",0x40);break;
   case Fault:bf("boardFault",1);break;case Reset:bf("boardReset",1);break;case VideoError:bf("videoError",0xdead,4);break;
   case Frame:set("pendingFrames",11);break;case Tick:set("liveTicks",1);break;
   case ClockFrame:cf("frame",9);break;case CreditDebt:cf("credit",50);break;case Quit:set("quitRequested",1,1);break;
   case Drain:set("nativeShortDrained",1);break;case Shuffle:wr(sym("shuffleQueue")+sym("shuffleCount"),4,1);break;
   case ShuffleActive:set("shuffleActive",1,1);break;case ShuffleQueued:set("shuffleQueued",1,1);break;
   case ShufflePointer:set("nativeShuffleNextPointer",ram);break;
   case Hold:bf("videoHold",1);bf("videoStatus",128);break;case Display:wr(sym("screen")+sym("screenPending"),4,0);break;
   case PresentDue:set("lastPresentCycle",63840000);break;case CardDue:wr(card+sym("cardHits"),4,8);break;
   case LateFrame:case LateQuit:case Format:break;default:assert(false);
  }
  bool sourceAllowed=true;unsigned expectedStatus=0x23;
  if(sourceKind==1 || sourceKind==2){
   const bool second=sourceKind==2;
   bf(second?"piacontrol1":"piacontrol0",sourceControl);
   bf(second?"piaflags1":"piaflags0",sourceFlags);
   const bool irq=((sourceFlags&128)&&(sourceControl&1)) ||
                  ((sourceFlags&64)&&(sourceControl&8)&&!(sourceControl&32));
   const bool priority=second && (sourceFlags&64) && (sourceControl&8);
   sourceAllowed=!irq && !priority;
  }else if(sourceKind==3){
   bf("serialControl",sourceControl);bf("serialRead",sourceFlags,4);
   sourceAllowed=(sourceControl&3)==3 ||
      ((sourceControl&96)!=32 && (!(sourceControl&128) || !sourceFlags));
  }else if(sourceKind==4){
   bf("videoStatus",sourceFlags);bf("videoEnable",sourceControl);
   expectedStatus=(sourceFlags&240)|3;
   sourceAllowed=(expectedStatus&sourceControl)!=0;
  }
  if(bad==ClockFrame || bad==CreditDebt){set("nativeShortGuest",0);set("nativeShortNominal",0);}
  set("nativeVirtualUsp",ram+0x3500);set("nativeVirtualSsp",ssp);wr(rom+0x100,4,target);
  unsigned regs[15];for(unsigned r=0;r<15;++r){regs[r]=0x12345000+r;wr(sym("nativeRegisters")+r*4,4,regs[r]^0xabcdef00);}
  wr(sym("nativeRegisters")+60,4,0xdeadbeef);wr(sym("nativeRegisters")+64,4,0xbeefdead);wr(sym("nativeRegisters")+68,2,virtualSr);
  unsigned frameSp=(supervisor?usp:ssp)-6;
  if(frameSp>=ram && frameSp<=ram+0x40000)for(unsigned i=0;i<6;++i)wr(frameSp+i,1,0xa5);
  auto beforeRegs=std::vector<uint8_t>(mem.begin()+sym("nativeRegisters"),mem.begin()+sym("nativeRegisters")+70);
  auto beforeTime=timeState();
  // Original dispatcher charges once through this same already-validated clock.
  // Compare actual linked execution, then restore the pre-charge state.
  m68k_set_reg(M68K_REG_SP,stack);wr(stack,4,stop);m68k_set_reg(M68K_REG_PC,sym("nativeClockPause"));unsigned steps=0;
  while(m68k_get_reg(nullptr,M68K_REG_PC)!=stop && ++steps<5000)m68k_execute(1);
  assert(steps<5000);auto onceTime=timeState();restoreTime(beforeTime);
  unsigned format=bad==Format?0x2008:0x28;
  m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_USP,usp);m68k_set_reg(M68K_REG_SP,stack);
  for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),regs[r]);
  if(wrapper){
   for(unsigned i=0;i<4;++i)wr(stack+i*4,4,regs[i<2?i:i+6]);wr(stack+16,2,physical);wr(stack+18,4,pc);wr(stack+22,2,format);
   m68k_set_reg(M68K_REG_PC,sym("nativeVideoIrqTry"));
  }else{wr(stack,4,stop);wr(stack+4,4,pc);wr(stack+8,4,usp);wr(stack+12,4,physical);m68k_set_reg(M68K_REG_PC,sym("nativeTryVideoIrq"));}
  bool late=false;steps=0;unsigned clockCalls=0;stores.clear();watching=true;
  while(++steps<10000){
   unsigned current=m68k_get_reg(nullptr,M68K_REG_PC);
   if(wrapper?(current==target || current==sym("nativeShortPromote")):current==stop)break;
   if(current==sym("nativeClockPause"))++clockCalls;
   unsigned op=rd(current,2);
   if(!late && (bad==LateFrame || bad==LateQuit) && (m68k_get_reg(nullptr,M68K_REG_SR)&0x700)==0 && (op&0xfff8)==0x46c0){
    watching=false;if(bad==LateFrame)set("pendingFrames",11);else set("quitRequested",1,1);watching=true;late=true;
   }
   m68k_execute(1);
  }
  watching=false;assert(steps<10000);
  bool okay=bad==Good && level<5 && sourceAllowed;
  if(bad==Format && (!wrapper || cpu==M68K_CPU_TYPE_68000))okay=level<5;
  bool got=wrapper?m68k_get_reg(nullptr,M68K_REG_PC)==target:m68k_get_reg(nullptr,M68K_REG_D0)!=0;
  if(got!=okay){std::fprintf(stderr,"IRQ admission mismatch cpu=%u flags=%u ipl=%u super=%u bad=%u wrapper=%u charge=%u got=%u pc=%x\n",cpu,flags,level,supervisor,bad,wrapper,charge,got,m68k_get_reg(nullptr,M68K_REG_PC));return false;}
  assert(clockCalls<=1);
  auto expectedTime=clockCalls?onceTime:beforeTime;
  if(wrapper && okay){expectedTime[expectedTime.size()-2]=0;expectedTime.back()=1;}
  if(timeState()!=expectedTime){
   std::fprintf(stderr,"IRQ timing mismatch cpu=%u flags=%u ipl=%u super=%u bad=%u wrapper=%u charge=%u calls=%u\n",cpu,flags,level,supervisor,bad,wrapper,charge,clockCalls);
   auto actual=timeState();for(unsigned i=0;i<actual.size();++i)if(actual[i]!=expectedTime[i])std::fprintf(stderr," byte%u actual=%u expected=%u\n",i,actual[i],expectedTime[i]);return false;
  }
  if(bad==LateFrame || bad==LateQuit){assert(late && clockCalls==1);++races;}
  unsigned expectedSr=(virtualSr&~31)|(physical&31);
  if(okay){
   ++admitted;assert(rd(frameSp,2)==expectedSr && rd(frameSp+2,4)==pc);
   assert(get("nativeVirtualUsp")== (supervisor?ram+0x3500:usp));assert(get("nativeVirtualSsp")==ssp);
   assert(rd(sym("nativeRegisters")+60,4)==frameSp && rd(sym("nativeRegisters")+64,4)==target);
   assert(rd(sym("nativeRegisters")+68,2)==((expectedSr|0x2000)&~0x700u|0x500));
   assert(get("nativeInterrupts")==18 && get("liveIrqActive",1)==1 && get("uninterruptedPoll",1)==0);
   assert(get("nativePhysicalSr",2)==physical && get("nativePhysicalResume",2)==flags && get("nativeLastPc")==0x2ebc);
   assert(get("nativeShortPending",2)==1 && get("nativeCachedVideoStatus",1)==expectedStatus);
   if(s.count("nativeVideoIrqHits"))assert(get("nativeVideoIrqHits")==8);
  }else{
   assert(std::equal(beforeRegs.begin(),beforeRegs.end(),mem.begin()+sym("nativeRegisters")));
   assert(get("nativeVirtualUsp")==ram+0x3500 && get("nativeVirtualSsp")==ssp && get("nativeInterrupts")==17);
   if(frameSp>=ram && frameSp<=ram+0x40000)for(unsigned i=0;i<6;++i)assert(rd(frameSp+i,1)==0xa5);
  }
  assert(get("nativeInstructions")==0xabcdef);
  for(unsigned r=wrapper?0:2;r<15;++r)if(wrapper || (r!=8 && r!=9))assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==regs[r]);
  if(wrapper){
   assert(m68k_get_reg(nullptr,M68K_REG_USP)==(okay?frameSp:usp));
   assert(m68k_get_reg(nullptr,M68K_REG_SP)==(okay?frameSp:stack+16));
   if(okay){assert(m68k_get_reg(nullptr,M68K_REG_SR)==flags);assert(get("nativeClockResumePc")==target && get("nativeClockRunning",2)==1);}
   assert(rd(stack+22,2)==format && timerWrites==unsigned(okay));
  }else assert(m68k_get_reg(nullptr,M68K_REG_SP)==stack+4);
  // All stores outside the bounded C/service stack and time bookkeeping must
  // be the precise virtual exception-frame/metadata stores on acceptance.
  std::vector<std::pair<unsigned,unsigned>> allowed=timeFields;
  if(okay){
   allowed.push_back({frameSp,6});allowed.push_back({sym("nativeRegisters")+60,10});
   for(const char*n:{"nativeInterrupts","nativeLastPc"})allowed.push_back({sym(n),4});
   for(const char*n:{"nativePhysicalSr","nativePhysicalResume","nativeShortPending"})allowed.push_back({sym(n),2});
   for(const char*n:{"liveIrqActive","uninterruptedPoll","nativeCachedVideoStatus"})allowed.push_back({sym(n),1});
   if(!supervisor)allowed.push_back({sym("nativeVirtualUsp"),4});
   if(s.count("nativeVideoIrqHits"))allowed.push_back({sym("nativeVideoIrqHits"),4});
   if(wrapper)allowed.push_back({sym("nativeClockResumePc"),4});
  }
  for(unsigned store:stores){bool permitted=store>=stack-2048 && store<stack+24;
   for(auto range:allowed)permitted|=store>=range.first && store-range.first<range.second;
   if(!permitted){std::fprintf(stderr,"unexpected IRQ store %x bad=%u okay=%u\n",store,bad,okay);return false;}
  }
  // The rejection path is already charged: another pause must be inert.
  set("nativeClockRunning",0,2);
  auto paused=timeState();m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,stack);wr(stack,4,stop);m68k_set_reg(M68K_REG_PC,sym("nativeClockPause"));steps=0;
  while(m68k_get_reg(nullptr,M68K_REG_PC)!=stop && ++steps<5000)m68k_execute(1);
  assert(steps<5000);if(clockCalls)assert(timeState()==paused);else assert(timeState()==onceTime);
  ++cases;return true;
 };
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})for(unsigned flags=0;flags<32;++flags)
 for(unsigned level=0;level<8;++level)for(bool supervisor:{false,true})for(bool wrapper:{false,true})
 for(unsigned edge=0;edge<4;++edge)for(unsigned charge=0;charge<3;++charge)
  if(!run(cpu,flags,level,supervisor,Good,wrapper,charge,edge))return 1;
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})for(unsigned bad=1;bad<LastBad;++bad)
 for(bool supervisor:{false,true})for(bool wrapper:{false,true})for(unsigned flags:{0u,4u,8u,16u,31u})
  if(!run(cpu,flags,0,supervisor,Bad(bad),wrapper,(bad==ClockFrame || bad==CreditDebt)?0:1,0))return 1;
 // Exhaustive concrete source enables/flags against the independent priority
 // predicate, executing the linked admission helper on both CPU types.
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020}){
  for(sourceKind=1;sourceKind<=4;++sourceKind)
   for(sourceControl=0;sourceControl<256;++sourceControl)
    for(sourceFlags=0;sourceFlags<(sourceKind==3?2u:256u);++sourceFlags)
     if(!run(cpu,0,0,false,Good,false,1,0))return 1;
 }
 sourceKind=0;
 std::printf("PASS: %u linked IRQ cases, %u admissions, %u late frame/quit races; CCR/IPL, both virtual stacks, every guard, exact stores, return and no double charge\n",cases,admitted,races);
}
