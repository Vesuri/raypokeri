// T13 joined handler: independently assembled oracles against linked native assembly.
#include "musashi/m68k.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#define require(x) do {if(!(x)){fprintf(stderr,"check failed line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static std::array<unsigned char,0x200000> mem;
static constexpr unsigned frame=0x120000,desc=0x140000,port=0x180000,fields=0x190000,selectFields=0x1b0000,stack=0x1c0000;
static bool oracle=false;static unsigned selector[3];
static unsigned rd(unsigned a,unsigned n){require(a<=mem.size()-n);unsigned v=0;while(n--)v=v*256+mem[a++];return v;}
static void wr(unsigned a,unsigned n,unsigned v){require(a<=mem.size()-n);while(n){--n;mem[a+n]=v;v>>=8;}}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return rd(a,1);}unsigned m68k_read_memory_16(unsigned a){return rd(a,2);}unsigned m68k_read_memory_32(unsigned a){return rd(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){if(oracle && a==port){selector[0]=v;selector[1]=selector[2]=0;}else wr(a,1,v);}
void m68k_write_memory_16(unsigned a,unsigned v){wr(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){wr(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return rd(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return rd(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return rd(a,4);}
void pokeri_exception(unsigned v){fprintf(stderr,"unexpected exception %u\n",v);exit(1);}
}
struct State {unsigned regs[16],pc,sr,cycles;};
int main(int argc,char **argv){
 require(argc==3);std::map<std::string,unsigned>s;std::ifstream meta(argv[2]);std::string name;unsigned value;
 while(meta>>name>>value)s[name]=value;
 auto sym=[&](const std::string &n){require(s.count(n));return s[n];};
 auto set=[&](const char*n,unsigned v,unsigned bytes=4){wr(sym(n),bytes,v);};
 std::ifstream in(argv[1],std::ios::binary);auto number=[&](){unsigned v=0;for(unsigned i=0;i<4;++i){int c=in.get();require(c>=0);v=v*256+c;}return v;};
 unsigned segments=number();while(segments--){unsigned p=number(),n=number();require(p+n<frame);in.read((char*)mem.data()+p,n);require(unsigned(in.gcount())==n);}
 const unsigned code=sym("oracle_setup");const bool counts=sym("nativeLiveCounterMode")!=0;
 const std::vector<unsigned> stops={sym("nativeShortControlPromote"),sym("nativeShortNoControlDue"),sym("nativeShortVideoGuard"),
  sym("nativeShortStatusGuard"),sym("nativeShortHandlerTail"),sym("nativeVideoIrqGuestResume")};
 auto run=[&](unsigned start){
  m68k_set_reg(M68K_REG_PC,start);unsigned steps=0;
  for(;;){unsigned pc=m68k_get_reg(nullptr,M68K_REG_PC);for(unsigned stop:stops)if(pc==stop)return pc;require(++steps<1000);m68k_execute(1);}
 };
 m68k_init();unsigned setupCases=0,deliveryCases=0;
 // Joined setup: the selector MOVE.B, its event check, then $2E3A-$2E56.
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})for(unsigned shape=0;shape<5;++shape)
 for(unsigned flags=0;flags<32;++flags)for(unsigned bad=0;bad<6;++bad)
 for(unsigned ring:{0x198000u,0x7ffffff8u,0xfffffff0u}){
  unsigned initial[16];for(unsigned i=0;i<16;++i)initial[i]=0x76540000+i*0x101;
  initial[8]=port;initial[15]=0x160100;initial[14]=fields+30682;
  unsigned ramFirst=fields,ramLast=fields+0x1000;
  if(bad==1)++initial[14];
  if(bad==2)ramFirst=fields+2;
  if(bad==3)ramLast=fields+2;
  if(bad==4)ramLast=fields+158;
  if(bad==5)initial[14]=30680; // first EA wraps to $FFFFFFFE
  const unsigned end=ring+16;
  unsigned cursor=ring+4,producer=ring+8;
  if(shape==1)producer=cursor;
  if(shape==2){cursor=end;producer=ring+8;}
  if(shape==3){cursor=end;producer=ring;}
  if(shape==4){cursor=end;producer=end;}
  wr(fields,4,producer);wr(fields+4,4,cursor);wr(fields+152,4,ring);wr(fields+156,4,end);
  std::vector<State> states;unsigned cycles=0;
  oracle=true;selector[0]=0x91;selector[1]=selector[2]=1;
  m68k_set_cpu_type(M68K_CPU_TYPE_68000);m68k_set_reg(M68K_REG_SR,0x2700|flags);
  for(unsigned r=0;r<16;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),initial[r]);
  m68k_set_reg(M68K_REG_PC,code);
  const bool badRam=bad!=0;
  for(;;){
   unsigned pc=m68k_get_reg(nullptr,M68K_REG_PC);
   if(pc==sym("oracle_setup_feed") || pc==sym("oracle_setup_empty") || pc==sym("oracle_setup_exit"))break;
   if(badRam && pc==code+4)break;
   cycles+=m68k_execute(1);State st;
   for(unsigned r=0;r<16;++r)st.regs[r]=m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r));
   st.pc=m68k_get_reg(nullptr,M68K_REG_PC);st.sr=m68k_get_reg(nullptr,M68K_REG_SR);st.cycles=cycles;states.push_back(st);require(states.size()<20);
  }
  oracle=false;require(!states.empty());
  // 0 none; 1/2 frame or urgent work at the selector boundary; 3 a frame
  // arriving inside the setup, which the next endpoint must still see.
  for(unsigned event=0;event<4;++event)for(unsigned mask:{1u,3u})for(unsigned shuffle=0;shuffle<2;++shuffle){
   set("nativeDiagnostic",0,2);set("pendingFrames",0);set("seenFrames",0);set("nativeShortPending",0,2);
   set("nativeInstructions",counts?1:0);set("nativeShortNominal",12);set("nativeShortCalls",0);set("nativeProfileEnabled",0,2);
   set("nativeRamBegin",ramFirst);set("nativeRamEnd",ramLast);set("nativeHandlerFeed",desc+32);set("nativeHandlerEmpty",desc+64);set("nativeFeedTarget",sym("oracle_setup_exit"));
   set("nativeFeedInlineCount",9);set("nativeFeedHeaderGrant",9);set("nativeRasterGrantActive",9);
   set("nativeShuffleNextPointer",shuffle?initial[9]:0);set("nativeClockResumePc",0);
   set("nativeVideoSelector",selectFields);wr(sym("nativeVideoSelector")+4,4,selectFields+2);wr(sym("nativeVideoSelector")+8,4,selectFields+3);
   wr(selectFields,1,0x91);wr(selectFields+1,1,0xa5);wr(selectFields+2,1,1);wr(selectFields+3,1,1);wr(selectFields+4,1,0x5a);
   wr(desc,4,code);wr(desc+12,4,0);wr(desc+26,2,mask);wr(desc+32,4,sym("oracle_setup_feed"));wr(desc+64,4,sym("oracle_setup_empty"));
   for(unsigned i=0;i<4;++i)wr(frame+i*4,4,initial[i<2?i:i+6]);
   wr(frame+16,2,0x0700|flags);wr(frame+18,4,code);wr(frame+22,2,0x28);
   m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,frame);m68k_set_reg(M68K_REG_USP,initial[15]);
   for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),initial[r]);
   m68k_set_reg(M68K_REG_A1,desc);m68k_set_reg(M68K_REG_D1,0);
   m68k_set_reg(M68K_REG_PC,sym("nativeShortHandlerJoinedSetup"));unsigned steps=0,pc;
   for(;;){
    pc=m68k_get_reg(nullptr,M68K_REG_PC);bool stop=false;for(unsigned x:stops)stop|=pc==x;if(stop)break;
    require(++steps<1000);
    if(pc==sym("nativeHandlerJoinedSelectBoundary")){if(event==1)set("pendingFrames",1);if(event==2)set("nativeShortPending",1,2);}
    if(pc==sym("nativeHandlerJoinedQueue") && event==3)set("pendingFrames",1);
    m68k_execute(1);
   }
   const bool promote=event==1 || event==2 || (shuffle && (mask&2));
   const State &select=states[0];
   require(rd(selectFields,1)==0 && rd(selectFields+2,2)==0 && rd(selectFields+1,1)==0xa5 && rd(selectFields+4,1)==0x5a);
   require(!rd(sym("nativeFeedInlineCount"),4) && !rd(sym("nativeFeedHeaderGrant"),4) && !rd(sym("nativeRasterGrantActive"),4));
   if(rd(sym("nativeShortPending"),2)!=(event==2?1u:shuffle?2u:0u)){fprintf(stderr,"pending=%x event=%u shuffle=%u mask=%u shape=%u bad=%u flags=%u pc=%x\n",rd(sym("nativeShortPending"),2),event,shuffle,mask,shape,bad,flags,pc);return 1;}
   require(m68k_get_reg(nullptr,M68K_REG_SP)==frame && m68k_get_reg(nullptr,M68K_REG_USP)==initial[15] && rd(frame+22,2)==0x28);
   for(unsigned r=2;r<15;++r)if(r!=8 && r!=9)require(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==initial[r]);
   if(promote || badRam){
    // Selector complete; nothing of the setup has been executed or charged.
    require(pc==sym(promote?"nativeShortControlPromote":"nativeShortNoControlDue"));
    require(rd(frame+18,4)==code+4 && (rd(frame+16,2)&31)==(select.sr&31) && rd(sym("nativeClockResumePc"),4)==code+4);
    require(rd(sym("nativeShortNominal"),4)==12 && rd(sym("nativeInstructions"),4)==(counts?1:0));
    for(unsigned i=0;i<4;++i)require(rd(frame+i*4,4)==initial[i<2?i:i+6]);
    ++setupCases;continue;
   }
   const State &expected=states.back();
   if(rd(frame+18,4)!=expected.pc || (rd(frame+16,2)&31)!=(expected.sr&31) || rd(sym("nativeShortNominal"),4)!=expected.cycles){
    fprintf(stderr,"joined setup cpu=%u shape=%u flags=%u bad=%u event=%u pc=%x/%x sr=%x/%x cycles=%u/%u\n",cpu,shape,flags,bad,event,
     rd(frame+18,4),expected.pc,rd(frame+16,2),expected.sr,rd(sym("nativeShortNominal"),4),expected.cycles);return 1;
   }
   require((rd(frame+16,2)&0xffe0)==0x700 && rd(sym("nativeClockResumePc"),4)==expected.pc);
   require(rd(sym("nativeInstructions"),4)==(counts?states.size():0) && rd(sym("pendingFrames"),4)==(event==3?1:0));
   for(unsigned i=0;i<4;++i)require(rd(frame+i*4,4)==expected.regs[i<2?i:i+6]);
   if(expected.pc==sym("oracle_setup_feed"))require(pc==sym("nativeShortStatusGuard") && m68k_get_reg(nullptr,M68K_REG_A1)==desc+32);
   else if(expected.pc==sym("oracle_setup_empty"))require(pc==sym("nativeShortVideoGuard") && m68k_get_reg(nullptr,M68K_REG_A1)==desc+64);
   else require(pc==sym("nativeShortHandlerTail"));
   if(pc!=sym("nativeShortHandlerTail"))require(m68k_get_reg(nullptr,M68K_REG_A0)==expected.pc);
   ++setupCases;
  }
 }
 // Joined delivery: MOVEM.L D0-D1/A0-A1,-(SP) and MOVEA.L #port,A0 below the
 // already-built exception frame, then the unchanged $2E30 entry guard.
 const unsigned vector=0x10a000,entry=desc+96;
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})for(unsigned flags=0;flags<32;++flags)
 for(unsigned variant=0;variant<6;++variant){
  unsigned initial[16];for(unsigned i=0;i<16;++i)initial[i]=0x13570000+i*0x1011;
  unsigned ssp=stack+0x100,ramFirst=stack,joined=vector,target=vector;
  if(variant==1)ssp=stack+16;          // MOVEM reaches the first RAM byte
  if(variant==2)ssp=stack+14;          // would write below RAM
  if(variant==3)joined=0;              // not prepared
  if(variant==4)target=vector+2;       // another vector target
  if(variant==5){ssp=8;ramFirst=0;}    // SSP-16 borrows
  const bool admit=variant<2;
  std::vector<unsigned char> before(mem.begin()+stack-0x40,mem.begin()+stack+0x200);
  unsigned expectedSp=0,expectedA0=0,expectedCycles=0;std::array<unsigned char,16> pushed{};
  if(admit){
   oracle=true;m68k_set_cpu_type(M68K_CPU_TYPE_68000);m68k_set_reg(M68K_REG_SR,0x2700|flags);
   for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),initial[r]);
   m68k_set_reg(M68K_REG_SP,ssp);m68k_set_reg(M68K_REG_PC,sym("oracle_delivery"));
   while(m68k_get_reg(nullptr,M68K_REG_PC)!=sym("oracle_delivery_end"))expectedCycles+=m68k_execute(1);
   oracle=false;expectedSp=m68k_get_reg(nullptr,M68K_REG_SP);expectedA0=m68k_get_reg(nullptr,M68K_REG_A0);
   require((m68k_get_reg(nullptr,M68K_REG_SR)&31)==flags);
   for(unsigned i=0;i<16;++i)pushed[i]=mem[expectedSp+i];
   std::copy(before.begin(),before.end(),mem.begin()+stack-0x40);
  }
  set("nativeShortNominal",100);set("nativeInstructions",counts?7:0);set("nativeClockResumePc",vector);
  set("nativeRamBegin",ramFirst);set("nativeRamEnd",stack+0x1000);
  set("nativeJoinedVector",joined);set("nativeJoinedA0",port);set("nativeJoinedEntry",entry);
  for(unsigned i=0;i<4;++i)wr(frame+i*4,4,initial[i<2?i:i+6]);
  wr(frame+16,2,flags);wr(frame+18,4,target);wr(frame+22,2,0x28);
  m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,frame);m68k_set_reg(M68K_REG_USP,ssp);
  for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),initial[r]);
  m68k_set_reg(M68K_REG_A0,ssp);m68k_set_reg(M68K_REG_D0,ssp);
  unsigned pc=run(sym("nativeHandlerJoinedDelivery"));
  for(unsigned r=2;r<15;++r)if(r!=8 && r!=9)require(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==initial[r]);
  require(m68k_get_reg(nullptr,M68K_REG_SP)==frame && rd(frame+16,2)==flags && rd(frame+22,2)==0x28);
  if(!admit){
   require(pc==sym("nativeVideoIrqGuestResume"));
   require(m68k_get_reg(nullptr,M68K_REG_USP)==ssp && m68k_get_reg(nullptr,M68K_REG_A0)==ssp && rd(frame+18,4)==target);
   for(unsigned i=0;i<4;++i)require(rd(frame+i*4,4)==initial[i<2?i:i+6]);
   require(rd(sym("nativeShortNominal"),4)==100 && rd(sym("nativeInstructions"),4)==(counts?7:0) && rd(sym("nativeClockResumePc"),4)==vector);
   require(std::equal(before.begin(),before.end(),mem.begin()+stack-0x40));
   ++deliveryCases;continue;
  }
  require(pc==sym("nativeShortStatusGuard"));
  require(m68k_get_reg(nullptr,M68K_REG_USP)==expectedSp && rd(frame+8,4)==expectedA0 && expectedA0==port);
  for(unsigned i=0;i<16;++i)require(mem[expectedSp+i]==pushed[i]);
  for(unsigned a=stack-0x40;a<stack+0x200;++a)if(a<expectedSp || a>=expectedSp+16)require(mem[a]==before[a-(stack-0x40)]);
  require(rd(frame,4)==initial[0] && rd(frame+4,4)==initial[1] && rd(frame+12,4)==initial[9]);
  require(rd(frame+18,4)==vector+10 && rd(sym("nativeClockResumePc"),4)==vector+10);
  require(m68k_get_reg(nullptr,M68K_REG_A0)==vector+10 && m68k_get_reg(nullptr,M68K_REG_A1)==entry);
  require(rd(sym("nativeShortNominal"),4)==100+expectedCycles && expectedCycles==52);
  require(rd(sym("nativeInstructions"),4)==(counts?9:0));
  ++deliveryCases;
 }
 printf("PASS: %u joined setup cases (both CPUs, all CCRs, empty/wrapped/signed queues, selector events, in-setup frames, shuffle marker, rejected RAM spans) and %u joined delivery cases\n",setupCases,deliveryCases);
}
