// Differential test of linked assembly against an independently assembled ring
// feeder. The oracle uses different branch sizes and a different field offset.
#include "musashi/m68k.h"
#include <array>
#include <cassert>
#include <cstdlib>
#undef assert
#define assert(condition) do { if(!(condition)){std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#condition);std::exit(1);} } while(0)
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <vector>
static std::array<unsigned char,1048576> memory;
static std::vector<unsigned> output;
static unsigned read(unsigned a,unsigned n){assert(a+n<=memory.size());unsigned v=0;while(n--)v=v*256+memory[a++];return v;}
static void write(unsigned a,unsigned n,unsigned v){assert(a+n<=memory.size());while(n){--n;memory[a+n]=v;v>>=8;}}
static const unsigned code=0x60000,source=0x70000,frame=0x80000,desc=0x90000,port=0xa0000;
static unsigned readyWords;
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return a==port?(output.size()<readyWords?2:0):read(a,1);}
unsigned m68k_read_memory_16(unsigned a){return read(a,2);}unsigned m68k_read_memory_32(unsigned a){return read(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){write(a,1,v);}void m68k_write_memory_16(unsigned a,unsigned v){if(a==port+2 || (a>=0x98000 && a<0x98080))output.push_back(v);write(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){write(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return read(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return read(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return read(a,4);}
void pokeri_exception(unsigned){assert(false && "unexpected exception");}
}
int main(int argc,char**argv){
 assert(argc==3 || argc==4);std::map<std::string,unsigned>s;std::ifstream meta(argv[2]);std::string name;unsigned address;
 while(meta>>name>>address)s[name]=address;
 auto sym=[&](const char*n){assert(s.count(n));return s[n];};
 FILE*f=fopen(argv[1],"rb");assert(f);auto word=[&](){unsigned n=0;for(int i=0;i<4;++i){int c=fgetc(f);assert(c>=0);n=n*256+c;}return n;};
 unsigned segments=word();while(segments--){unsigned a=word(),n=word();assert(a+n<source);assert(fread(memory.data()+a,1,n,f)==n);}fclose(f);
 auto set=[&](const char*n,unsigned v,unsigned size=4){write(sym(n),size,v);};auto get=[&](const char*n){return read(sym(n),4);};
 std::map<unsigned,unsigned>pcs;
 const char*labels[]={"oracle_head","oracle_head_exit","oracle_status","oracle_ready","oracle_write","oracle_end","oracle_wrap_branch","oracle_producer","oracle_producer_exit","oracle_wrap","oracle_again","oracle_exit"};
 unsigned offsets[]={0,2,4,8,10,14,16,18,20,22,26,42};
 for(unsigned i=0;i<12;++i)pcs[sym(labels[i])]=code+offsets[i];
 // Resolve hot breakpoints once, not with string/map lookups per CPU instruction.
 const unsigned pc_nativeFeedBoundary=sym("nativeFeedBoundary");
 const unsigned pc_nativeShortControlPromote=sym("nativeShortControlPromote");
 const unsigned pc_nativeShortLengthDone=sym("nativeShortLengthDone");
 const unsigned pc_nativeShortNoControlDue=sym("nativeShortNoControlDue");
 const unsigned pc_nativeFeedReplayContinue=sym("nativeFeedReplayContinue");
 const unsigned pc_nativeShortReplayStart=sym("nativeShortReplayStart");
 const unsigned pc_nativeShortVideoWriteValue=sym("nativeShortVideoWriteValue");
 const unsigned pc_nativeFeedLoopValueReady=sym("nativeFeedLoopValueReady");
 const unsigned pc_nativeFeedLoopCallModel=sym("nativeFeedLoopCallModel");
 m68k_init();unsigned checks=0;
 struct State{unsigned pc,sr,a1,cycles;std::vector<unsigned>words;};
 if(argc==3)for(unsigned ring: {2u,4u,7u})for(unsigned start=0;start<ring;++start)for(unsigned count=1;count<=ring;++count)
 for(unsigned ready: {0u,1u,3u,100u})for(unsigned flags=0;flags<32;flags+=1){
  unsigned initial[15];for(unsigned r=0;r<15;++r)initial[r]=0x34560000+r;
  initial[0]=source+ring*2;initial[1]=source+((start+count)%ring)*2;
  // Include the exact-end producer sentinel without wrapping in half the cases.
  if(start+count==ring && (flags&1))initial[1]=source+ring*2;
  initial[8]=port;initial[9]=source+start*2;initial[14]=source+0x800+30530;
  for(unsigned i=0;i<ring;++i)write(source+i*2,2,i==0?0:0x8001+i);
  write(source+0x800,4,source);readyWords=ready;output.clear();
  m68k_set_cpu_type(M68K_CPU_TYPE_68000);m68k_set_reg(M68K_REG_SR,0x2500|flags);m68k_set_reg(M68K_REG_SP,frame+0x2000);
  for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),initial[r]);
  m68k_set_reg(M68K_REG_A6,source+0x800);m68k_set_reg(M68K_REG_PC,sym("oracle_status"));
  std::vector<State>states;unsigned cycles=0;
  while(m68k_get_reg(nullptr,M68K_REG_PC)!=sym("oracle_exit")){
   unsigned pc=m68k_get_reg(nullptr,M68K_REG_PC);unsigned c=m68k_execute(1);
   // The independent oracle has word branches: not-taken Bcc costs 12
   // rather than the verified short form's 8; MOVEA (A6) costs 12 vs 16.
   unsigned next=m68k_get_reg(nullptr,M68K_REG_PC);
   if(pc==sym("oracle_wrap"))c+=4;
   if((pc==sym("oracle_ready")||pc==sym("oracle_head_exit")||pc==sym("oracle_wrap_branch")||pc==sym("oracle_producer_exit")) && next==pc+4)c-=4;
   cycles+=c;assert(pcs.count(next));
   states.push_back({pcs[next],m68k_get_reg(nullptr,M68K_REG_SR),m68k_get_reg(nullptr,M68K_REG_A1),cycles,output});assert(states.size()<200);
  }
  for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})for(unsigned diagnostic:{0u,1u})for(unsigned fast:{0u,1u,2u})for(unsigned inlineMode:{0u,1u,2u})for(unsigned stop=1;stop<=states.size();++stop){
   // The live fast tail has boundaries at device instructions and its exit;
   // every intermediate boundary remains tested in the general/replay tail.
   bool liveFast=fast && !diagnostic;
   auto deviceBoundary=[&](unsigned i){unsigned p=i?states[i-1].pc:code+4;return p==code+4||p==code+8||p==code+10;};
   if(liveFast && stop<states.size() && !deviceBoundary(stop-1))continue;
   unsigned targetBoundary=0;
   for(unsigned i=0;i<stop;++i)if(!liveFast || deviceBoundary(i))++targetBoundary;
   unsigned boundaryStop=targetBoundary+(liveFast && !deviceBoundary(stop-1));
   output.clear();m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,frame);
   for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),initial[r]);
   m68k_set_reg(M68K_REG_PC,sym("nativeShortFeedRead"));m68k_set_reg(M68K_REG_A1,desc);
   for(unsigned i=0;i<4;++i)write(frame+i*4,4,initial[i<2?i:i+6]);
   write(frame+16,2,0x2500|flags);write(frame+18,4,code+4);write(frame+22,2,0x28);
   write(desc,4,code+4);write(desc+4,4,port);write(desc+8,2,2);write(desc+10,2,12);write(desc+20,4,sym("nativeShortFeedRead"));write(desc+28,4,desc+32);
   write(desc+32,4,code+10);write(desc+36,4,port+2);write(desc+40,2,0x0807);write(desc+42,2,16);write(desc+52,4,sym("nativeShortFeedLoopWrite"));write(desc+56,2,4);write(desc+60,4,desc);
   set("nativeFeedInlineCount",0);set("nativeFeedInlineWords",0);set("nativeFeedHeaderGrant",0);
   set("nativeFeedLoopFast",fast!=0,2);set("nativeRegisterFeedEnabled",fast==2,2);set("nativeDiagnostic",diagnostic,2);set("nativeCachedVideoStatus",ready?2:0,1);set("nativeFeedTarget",code+42);
   set("nativeRomBegin",code);set("nativeRomEnd",code+0x1000);set("nativeRamBegin",source);set("nativeRamEnd",source+0x1000);
   set("nativeShortPending",1,2);set("pendingFrames",0);set("seenFrames",0);set("nativeInstructions",1);set("nativeShortCalls",0);set("nativeShortNominal",12);
   // Also let a real shuffle marker, rather than the injected pending bit,
   // stop selected post-write boundaries inside the register-resident loop.
   bool marker=!diagnostic && (flags&8) && states[stop-1].pc==code+14;
   set("nativeShuffleNextPointer",marker?states[stop-1].a1:0);
   unsigned boundaries=0,steps=0,pc=0;
   while((pc=m68k_get_reg(nullptr,M68K_REG_PC))!=pc_nativeShortControlPromote && pc!=pc_nativeShortLengthDone && pc!=pc_nativeShortNoControlDue && steps++<10000){
    if(pc==pc_nativeFeedBoundary){
     ++boundaries;
     if(!diagnostic && !marker && boundaries==boundaryStop){if(flags&1)set("nativeShortPending",2,2);else set("pendingFrames",1);}
    }
    if(pc==pc_nativeFeedReplayContinue||pc==pc_nativeShortReplayStart||pc==pc_nativeShortVideoWriteValue){
     unsigned sp=m68k_get_reg(nullptr,M68K_REG_SP),result=1;
     if(pc==pc_nativeFeedReplayContinue)result=boundaries<boundaryStop;
     else if(pc==pc_nativeShortReplayStart){
      unsigned p=read(sp+4,4);assert(p==code+4||p==code+10);set("nativeInstructions",get("nativeInstructions")+1);
     }else{assert(read(sp+4,4)==port+2&&read(sp+12,4)==7);result=read(sp+8,4);output.push_back(result);set("nativeCachedVideoStatus",output.size()<ready?2:0,1);
      unsigned available=ready>output.size()+1?ready-unsigned(output.size())-1:0;
      set("nativeFeedInlineCount",inlineMode==1 && !diagnostic?(available>2?2:available):0);
      set("nativeFeedInlineWord",0x98000+unsigned(output.size())*2);set("nativeFeedInlinePending",0x98100);set("nativeFeedInlineHigh",0x98104);
      set("nativeFeedHeaderGrant",inlineMode==2 && !diagnostic && available>=2);
      set("nativeFeedInlineLength",0x98108);set("nativeFeedFormats",0x98200);
      // Synthetic shared decoder: AMOVE group accepts its low bits here to
      // exercise headers with the oracle's arbitrary words; group zero rejects.
      for(unsigned g=0;g<64;++g){write(0x98200+g*4,2,g?3:0);write(0x98202+g*4,2,0);}
      write(0x98100,4,unsigned(output.size()));write(0x98104,1,result>>8);}
     m68k_set_reg(M68K_REG_D0,result);m68k_set_reg(M68K_REG_D1,0xdeadbeef);m68k_set_reg(M68K_REG_A0,0xabcdef00);m68k_set_reg(M68K_REG_A1,0x76543210);
     m68k_set_reg(M68K_REG_PC,read(sp,4));m68k_set_reg(M68K_REG_SP,sp+4);continue;
    }
    m68k_execute(1);
   }
   auto&e=states[stop-1];
   if(steps>=10000||boundaries!=targetBoundary||read(frame+18,4)!=e.pc||read(frame+16,2)!=e.sr||read(frame+12,4)!=e.a1||output!=e.words||(!diagnostic&&get("nativeShortNominal")!=e.cycles)){
    fprintf(stderr,"loop mismatch ring=%u start=%u count=%u ready=%u flags=%u cpu=%u diag=%u stop=%u boundaries=%u pc=%x/%x sr=%x/%x a1=%x/%x cycles=%u/%u steps=%u\n",ring,start,count,ready,flags,cpu,diagnostic,stop,boundaries,read(frame+18,4),e.pc,read(frame+16,2),e.sr,read(frame+12,4),e.a1,get("nativeShortNominal"),e.cycles,steps);return 1;
   }
   for(unsigned i=0;i<3;++i)assert(read(frame+i*4,4)==initial[i<2?i:8]);
   for(unsigned r=2;r<15;++r)if(r!=8&&r!=9)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==initial[r]);
   assert(m68k_get_reg(nullptr,M68K_REG_SP)==frame&&read(frame+22,2)==0x28);
   if(diagnostic)assert(get("nativeInstructions")==stop);
   ++checks;
  }
 }
 set("nativeShuffleNextPointer",0);
 // Exercise the actual header body with all input words and synthetic decoder
 // metadata. Model tests independently cover the real command table and state.
 unsigned headers=0;
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})for(unsigned value=0;value<65536;++value){
  const int lengths[]={0,1,2,3,7,-1,-2};unsigned group=value>>10;
  int length=lengths[group%7];unsigned reserved=(group&1)?0x30:0x201;
  bool accepted=length!=0 && length!=1 && !(value&reserved);
  output.clear();m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,frame);
  m68k_set_reg(M68K_REG_D1,value);m68k_set_reg(M68K_REG_A1,desc+32);
  m68k_set_reg(M68K_REG_PC,sym("nativeShortFeedLoopWrite"));write(frame+12,4,source);
  set("nativeFeedInlineCount",0);set("nativeFeedHeaderGrant",1);set("nativeFeedHeaderWords",0);
  set("nativeFeedInlineWord",0x98000);set("nativeFeedInlinePending",0x98100);
  set("nativeFeedInlineHigh",0x98104);set("nativeFeedInlineLength",0x98108);set("nativeFeedFormats",0x98200);
  set("nativeCachedVideoStatus",0x77,1);
  write(0x98100,4,0);write(0x98104,1,0x55);write(0x98108,4,0);
  write(0x98200+group*4,2,unsigned(length));write(0x98202+group*4,2,reserved);
  unsigned steps=0,pc;
  while((pc=m68k_get_reg(nullptr,M68K_REG_PC))!=pc_nativeFeedLoopValueReady && pc!=pc_nativeFeedLoopCallModel && steps++<100)m68k_execute(1);
  assert(steps<100 && (pc==pc_nativeFeedLoopValueReady)==accepted);
  assert(m68k_get_reg(nullptr,M68K_REG_SP)==frame-(accepted?0:4) && m68k_get_reg(nullptr,M68K_REG_A1)==desc+32);
  assert(read(frame+12,4)==source+2 && m68k_get_reg(nullptr,M68K_REG_D1)==value);
  if(accepted){
   assert(output==std::vector<unsigned>{value} && get("nativeFeedHeaderGrant")==0 && get("nativeFeedHeaderWords")==sym("nativeFeedCounterMode"));
   assert(read(0x98100,4)==1 && read(0x98104,1)==value>>8 && read(0x98108,4)==unsigned(length));
   assert(get("nativeFeedInlineCount")==unsigned(length>2?length-2:0));
   assert(get("nativeFeedInlineWord")==0x98002 && read(sym("nativeCachedVideoStatus"),1)==0x57);
   assert(m68k_get_reg(nullptr,M68K_REG_D0)==value);
  }else assert(output.empty() && !read(0x98100,4) && read(sym("nativeCachedVideoStatus"),1)==0x77);
  ++headers;
 }
 set("nativeFeedHeaderGrant",0);set("nativeFeedInlineCount",0);
 printf("PASS: %u linked header cases: reserved bits, invalid/single/variable/fixed lengths, exact fields and CED\n",headers);
 // Exact post-write promotion at a shuffle marker must not execute any tail
 // instruction, alter guest CCR/registers, or feed another word.
 unsigned markers=0;
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})for(unsigned flags=0;flags<32;++flags){
  m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,frame);
  m68k_set_reg(M68K_REG_A1,desc+32);m68k_set_reg(M68K_REG_PC,sym("nativeFeedLoopAfterWrite"));
  for(unsigned i=0;i<4;++i)write(frame+i*4,4,0x34560000+i);
  write(frame+12,4,source+4);write(frame+16,2,0x2500|flags);write(frame+18,4,code+14);
  set("nativeShuffleNextPointer",source+4);set("nativeDiagnostic",0,2);set("nativeShortPending",0,2);
  set("pendingFrames",0);set("seenFrames",0);set("nativeShortNominal",16);
  unsigned steps=0;while(m68k_get_reg(nullptr,M68K_REG_PC)!=pc_nativeShortControlPromote && steps++<100)m68k_execute(1);
  assert(steps<100 && read(frame+18,4)==code+14 && read(frame+16,2)==(0x2500|flags));
  assert(read(frame+12,4)==source+4 && get("nativeShortNominal")==16 && read(sym("nativeShortPending"),2)==2);
  for(unsigned i=0;i<3;++i)assert(read(frame+i*4,4)==0x34560000+i);
  assert(m68k_get_reg(nullptr,M68K_REG_SP)==frame);++markers;
 }
 set("nativeShuffleNextPointer",0);
 printf("PASS: %u shuffle-marker exits preserve post-write PC/CCR/cursor and guest work accounting\n",markers);
 unsigned guards=0;
 for(unsigned bad:{0u,1u,source-2,source+0xffeu,source+0xfffu,source+0x1000,0xfffffffeu,0xffffffffu})
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})for(unsigned diagnostic:{0u,1u})for(unsigned fast:{0u,1u,2u})for(unsigned flags=0;flags<32;++flags){
  m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,frame);
  m68k_set_reg(M68K_REG_A6,bad+30530);m68k_set_reg(M68K_REG_A1,desc+32);m68k_set_reg(M68K_REG_PC,sym("nativeFeedLoopAfterWrite"));
  write(frame,4,source+4);write(frame+4,4,source);write(frame+8,4,port);write(frame+12,4,source+4);
  write(frame+16,2,0x2500|flags);write(frame+18,4,code+14);write(desc+60,4,desc);
  set("nativeRamBegin",source);set("nativeRamEnd",source+0x1000);set("nativeDiagnostic",diagnostic,2);set("nativeFeedLoopFast",fast!=0,2);set("nativeRegisterFeedEnabled",fast==2,2);
  set("nativeShortPending",0,2);set("pendingFrames",0);set("seenFrames",0);set("nativeInstructions",1);set("nativeShortNominal",16);
  unsigned steps=0;
  while(m68k_get_reg(nullptr,M68K_REG_PC)!=pc_nativeShortControlPromote && steps++<1000){
   if(m68k_get_reg(nullptr,M68K_REG_PC)==pc_nativeFeedReplayContinue){
    unsigned sp=m68k_get_reg(nullptr,M68K_REG_SP);m68k_set_reg(M68K_REG_D0,1);m68k_set_reg(M68K_REG_D1,0xdeadbeef);m68k_set_reg(M68K_REG_A0,0xabcdef00);m68k_set_reg(M68K_REG_A1,0x76543210);
    m68k_set_reg(M68K_REG_PC,read(sp,4));m68k_set_reg(M68K_REG_SP,sp+4);continue;
   }
   m68k_execute(1);
  }
  assert(steps<1000 && read(frame+18,4)==code+22 && read(frame+16,2)==(0x2509|(flags&16)));
  assert(read(frame+12,4)==source+4 && m68k_get_reg(nullptr,M68K_REG_SP)==frame);
  assert(diagnostic?get("nativeInstructions")==5:get("nativeShortNominal")==44);++guards;
 }
 printf("PASS: %u invalid ring-start loads stop at the exact pre-load PC/CCR with no read or cursor change\n",guards);
 // Reaching a later word with an invalid source must stop before the read,
 // including a wrap whose pointer load is valid but whose value is invalid.
 unsigned sources=0;
 for(unsigned bad:{0u,1u,code-2,code+0xfffu,code+0x1000,source-2,source+0xfffu,source+0x1000,0xfffffffeu,0xffffffffu})
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})for(unsigned flags=0;flags<32;++flags){
  unsigned initial[15];for(unsigned r=0;r<15;++r)initial[r]=0x34560000+r;
  initial[0]=0xffffffffu;initial[1]=source+0x100;initial[8]=port;initial[9]=bad;initial[14]=source+0x800+30530;
  write(source+0x800,4,bad);readyWords=100;output.clear();
  m68k_set_cpu_type(M68K_CPU_TYPE_68000);m68k_set_reg(M68K_REG_SR,0x2500|flags);m68k_set_reg(M68K_REG_SP,frame+0x2000);
  for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),initial[r]);
  m68k_set_reg(M68K_REG_A6,source+0x800);m68k_set_reg(M68K_REG_PC,sym("oracle_end"));
  unsigned cycles=16,steps=0;
  while(m68k_get_reg(nullptr,M68K_REG_PC)!=sym("oracle_write") && steps++<30){
   unsigned pc=m68k_get_reg(nullptr,M68K_REG_PC),c=m68k_execute(1),next=m68k_get_reg(nullptr,M68K_REG_PC);
   if(pc==sym("oracle_wrap"))c+=4;
   if((pc==sym("oracle_ready")||pc==sym("oracle_head_exit")||pc==sym("oracle_wrap_branch")||pc==sym("oracle_producer_exit")) && next==pc+4)c-=4;
   cycles+=c;
  }
  assert(steps<30);unsigned expectedSr=m68k_get_reg(nullptr,M68K_REG_SR);
  for(unsigned mode:{0u,1u}){
   m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,frame);
   for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),initial[r]);
   m68k_set_reg(M68K_REG_A1,desc+32);m68k_set_reg(M68K_REG_PC,sym("nativeFeedLoopAfterWrite"));
   for(unsigned i=0;i<4;++i)write(frame+i*4,4,initial[i<2?i:i+6]);
   write(frame+16,2,0x2500|flags);write(frame+18,4,code+14);write(desc+60,4,desc);
   set("nativeRomBegin",code);set("nativeRomEnd",code+0x1000);set("nativeRamBegin",source);set("nativeRamEnd",source+0x1000);
   set("nativeDiagnostic",0,2);set("nativeFeedLoopFast",1,2);set("nativeRegisterFeedEnabled",mode,2);
   set("nativeShortPending",0,2);set("pendingFrames",0);set("seenFrames",0);set("nativeCachedVideoStatus",2,1);
   set("nativeShuffleNextPointer",0);set("nativeInstructions",1);set("nativeShortNominal",16);
   steps=0;while(m68k_get_reg(nullptr,M68K_REG_PC)!=pc_nativeShortControlPromote && steps++<1000)m68k_execute(1);
   assert(steps<1000 && read(frame+18,4)==code+10 && read(frame+16,2)==expectedSr);
   assert(read(frame+12,4)==bad && get("nativeShortNominal")==cycles && output.empty());
   assert(m68k_get_reg(nullptr,M68K_REG_SP)==frame);
   for(unsigned r=2;r<15;++r)if(r!=8&&r!=9)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==initial[r]);
   ++sources;
  }
 }
 printf("PASS: %u later-word source guards preserve the independent CPU's exact boundary, cycles and registers\n",sources);
 if(checks)printf("PASS: %u whole-feed cases, every instruction boundary, ring wrap, producer sentinel, WFR backpressure, CCR, nominal cycles, 68000/68020 and C ABI clobbers\n",checks);
}
