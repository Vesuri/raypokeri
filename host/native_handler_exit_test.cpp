// Assemble synthetic MOVE/MOVEM/RTE instructions as an independent CPU oracle.
// No game routine or ROM image is embedded in this test.
#include "musashi/m68k.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
static std::array<unsigned char,0x200000> mem;
static constexpr unsigned code=0x100000,frame=0x120000,desc=0x140000,ram=0x160000,end=ram+0x10000,port=0x180000,fields=0x190000;
static unsigned selectors[3];static bool oracle=false,tailMode=false;
static constexpr unsigned consumer=ram+0x8000;
static unsigned rd(unsigned a,unsigned n){assert(a<=mem.size()-n);unsigned v=0;while(n--)v=v*256+mem[a++];return v;}
static void wr(unsigned a,unsigned n,unsigned v){assert(a<=mem.size()-n);while(n){--n;mem[a+n]=v;v>>=8;}}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return rd(a,1);}unsigned m68k_read_memory_16(unsigned a){return rd(a,2);}unsigned m68k_read_memory_32(unsigned a){return rd(a,4);}
static void guestWrite(unsigned a){if(!oracle && a>=ram && a<end && !(tailMode && a==consumer)){std::fprintf(stderr,"unexpected guest RAM store %x\n",a);assert(false);}}
void m68k_write_memory_8(unsigned a,unsigned v){guestWrite(a);if(oracle && a==port){selectors[0]=v;selectors[1]=selectors[2]=0;}else wr(a,1,v);}
void m68k_write_memory_16(unsigned a,unsigned v){guestWrite(a);wr(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){guestWrite(a);wr(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return rd(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return rd(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return rd(a,4);}
void pokeri_exception(unsigned v){std::fprintf(stderr,"unexpected exception %u\n",v);assert(false);}
}
int main(int argc,char **argv){
 assert(argc==3);std::map<std::string,unsigned>s;std::ifstream meta(argv[2]);std::string name;unsigned value;
 while(meta>>name>>value)s[name]=value;
 tailMode=s.count("nativeShortHandlerTail")!=0;
 auto sym=[&](const char*n){assert(s.count(n));return s[n];};
 auto set=[&](const char*n,unsigned v,unsigned bytes=4){wr(sym(n),bytes,v);};
 std::ifstream in(argv[1],std::ios::binary);auto number=[&](){unsigned v=0;for(unsigned i=0;i<4;++i){int c=in.get();assert(c>=0);v=v*256+c;}return v;};
 unsigned segments=number();while(segments--){unsigned p=number(),n=number();assert(p+n<(tailMode?code+16:code));in.read((char*)mem.data()+p,n);assert(unsigned(in.gcount())==n);}
 // MOVE.B #3,(A0); MOVEM.L (A7)+,D0-D1/A0-A1; RTE.
 if(!tailMode){wr(code,2,0x10bc);wr(code+2,2,3);wr(code+4,2,0x4cdf);wr(code+6,2,0x0303);wr(code+8,2,0x4e73);}
 const bool counts=s.count("nativeLiveCounterMode") && s["nativeLiveCounterMode"];
 m68k_init();unsigned cases=0,restores=0,returns=0;
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})for(unsigned flags=0;flags<32;++flags)
 for(unsigned ipl=0;ipl<8;++ipl)for(unsigned returnFlags=0;returnFlags<32;++returnFlags)for(unsigned user=0;user<2;++user)
 for(unsigned due=0;due<(tailMode?9u:7u);++due)for(unsigned bad=0;bad<(tailMode?16u:12u);++bad){
  if(bad==7)continue; // formerly the disabled stack-switch case
  unsigned initial[15];for(unsigned r=0;r<15;++r)initial[r]=0x76543000+r;
  initial[8]=port;
  if(tailMode){
   initial[14]=consumer+30678;
   if(bad==12)++initial[14];
   if(bad==13)initial[14]=ram-2+30678;
   if(bad==14)initial[14]=end-2+30678;
   if(bad==15)initial[14]=30676; // EA wraps before the validated RAM span
   wr(consumer,4,0x11223344);
  }
  unsigned sp=ram+0x100,usp=ram+0x4000;
  if(bad==1)++sp;if(bad==2)sp=ram-2;if(bad==3)sp=end-14;if(bad==4)sp=0xfffffff8;
  if(bad==5)sp=end-20;if(bad==9)sp=end-22;if(bad==10)sp=ram;if(bad==11)sp=end-24;
  unsigned sr=(bad==8?0:0x2000)|(ipl<<8)|flags,returnedSr=(user?0:0x2000)|((7-ipl)<<8)|returnFlags;
  if(bad==6)returnedSr|=0x8000;
  if(sp>=ram && sp<=end-16 && !(sp&1))
   for(unsigned r=0;r<4;++r)wr(sp+r*4,4,0x13579000+r);
  if(sp>=ram && sp<=end-22 && !(sp&1)){
   wr(sp+16,2,returnedSr);wr(sp+18,4,code+0x100);
  }
  // Expected stopping instruction follows the existing conservative RTE guard.
  unsigned limit=3;
  if(bad>=1 && bad<=4)limit=1;
  else if(bad==5 || bad==6 || bad==8 || bad==9)limit=2;
  if(tailMode){++limit;if(bad>=12)limit=0;}
  unsigned event=due?(due+1)/2:(tailMode?5:4);if(event<limit)limit=event;
  selectors[0]=0x91;selectors[1]=selectors[2]=1;
  oracle=true;m68k_set_cpu_type(M68K_CPU_TYPE_68000);m68k_set_reg(M68K_REG_SR,0x2700);
  m68k_set_reg(M68K_REG_USP,usp);m68k_set_reg(M68K_REG_SR,sr);m68k_set_reg(M68K_REG_SP,sp);
  for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),initial[r]);
  m68k_set_reg(M68K_REG_PC,code);unsigned cycles=0;
  for(unsigned i=0;i<limit;++i)cycles+=m68k_execute(1);
  unsigned expected[16];for(unsigned r=0;r<16;++r)expected[r]=m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r));
  unsigned expectedPc=m68k_get_reg(nullptr,M68K_REG_PC),expectedSr=m68k_get_reg(nullptr,M68K_REG_SR);
  const unsigned expectedConsumer=tailMode?rd(consumer,4):0;
  if(tailMode)wr(consumer,4,0x11223344);
  oracle=false;
  set("nativeDiagnostic",0,2);set("pendingFrames",0);set("seenFrames",0);set("nativeShortPending",0,2);
  set("nativeInstructions",counts && !tailMode?1:0);set("nativeShortNominal",tailMode?0:12);set("nativeShortCalls",0);
  set("nativeFeedInlineCount",17);set("nativeFeedHeaderGrant",19);set("nativeProfileEnabled",0,2);
  set("nativeClockResumePc",code);set("nativeRamBegin",ram);set("nativeRamEnd",end);
  set("nativeVirtualUsp",usp);set("nativeVirtualSsp",ram+0x7000);
  wr(sym("nativeRegisters")+68,2,sr);
  wr(fields+1,1,0xa5);wr(fields+4,1,0x5a);
  set("nativeVideoSelector",fields);wr(sym("nativeVideoSelector")+4,4,fields+2);wr(sym("nativeVideoSelector")+8,4,fields+3);
  wr(fields,1,0x91);wr(fields+2,1,1);wr(fields+3,1,1);
  for(unsigned i=0;i<4;++i)wr(frame+i*4,4,initial[i<2?i:i+6]);
  wr(frame+16,2,0x0700|flags);wr(frame+18,4,code);wr(frame+22,2,0x28);
  wr(desc+4,4,port);wr(desc+28,4,desc+32);wr(desc+32,4,code+(tailMode?12:8));wr(desc+40,2,0x4002);wr(desc+42,2,20);
  wr(desc+52,4,sym("nativeShortControlRead"));wr(desc+56,2,0);wr(desc+58,2,3);
  if(tailMode){set("nativeHandlerTailPc",code);set("nativeHandlerTailExit",desc);}
  m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,frame);m68k_set_reg(M68K_REG_USP,sp);
  for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),initial[r]);
  m68k_set_reg(M68K_REG_D1,3);m68k_set_reg(M68K_REG_A1,desc);m68k_set_reg(M68K_REG_PC,sym(tailMode?"nativeShortHandlerTail":"nativeShortHandlerExit"));
  unsigned steps=0,pc;
  while((pc=m68k_get_reg(nullptr,M68K_REG_PC))!=sym("nativeShortControlPromote") && pc!=sym("nativeShortNoControlDue") && pc!=sym("nativeShortDecline") && steps++<500){
   unsigned boundary=pc==sym("nativeHandlerExitAddressBoundary")?1:pc==sym("nativeHandlerExitRestoreBoundary")?2:pc==sym("nativeShortLengthDone")?3:0;
   if(tailMode){if(boundary)++boundary;else if(pc==sym("nativeHandlerTailStoreBoundary"))boundary=1;}
   if(boundary==event){if(due&1)set("pendingFrames",1);else set("nativeShortPending",2,2);}
   m68k_execute(1);
  }
  unsigned actualSr=(rd(sym("nativeRegisters")+68,2)&~31)|(rd(frame+16,2)&31);
  if(steps>=500 || rd(frame+18,4)!=expectedPc || actualSr!=expectedSr || m68k_get_reg(nullptr,M68K_REG_USP)!=expected[15] || rd(sym("nativeShortNominal"),4)!=cycles){
   fprintf(stderr,"exit cpu=%u flags=%u return=%u user=%u due=%u bad=%u steps=%u done=%u pc=%x/%x sr=%x/%x sp=%x/%x cycles=%u/%u\n",cpu,flags,returnFlags,user,due,bad,steps,limit,rd(frame+18,4),expectedPc,actualSr,expectedSr,m68k_get_reg(nullptr,M68K_REG_USP),expected[15],rd(sym("nativeShortNominal"),4),cycles);return 1;
  }
  if(tailMode)assert(rd(consumer,4)==expectedConsumer);
  assert(rd(sym("nativeInstructions"),4)==(counts?limit:0));
  for(unsigned i=0;i<3;++i)assert(rd(fields+(i?i+1:0),1)==selectors[i]);
  for(unsigned i=0;i<4;++i)assert(rd(frame+i*4,4)==expected[i<2?i:i+6]);
  for(unsigned r=2;r<15;++r)if(r!=8 && r!=9)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==expected[r]);
  assert(rd(sym("nativeClockResumePc"),4)==expectedPc);
  assert(m68k_get_reg(nullptr,M68K_REG_SP)==frame && rd(frame+22,2)==0x28);
  assert((rd(frame+16,2)&0xffe0)==0x0700);
  if(limit==(tailMode?4u:3u) && user)assert(rd(sym("nativeVirtualSsp"),4)==sp+22);
  assert(rd(sym("nativeFeedInlineCount"),4)==0 && rd(sym("nativeFeedHeaderGrant"),4)==0);
  assert(rd(fields+1,1)==0xa5 && rd(fields+4,1)==0x5a);
  ++cases;restores+=limit>=(tailMode?3u:2u);returns+=limit==(tailMode?4u:3u);
 }
 printf("PASS: %u linked exit cases, %u restores, %u RTEs; both CPUs, all CCRs/IPLs, exact allowed guest RAM stores, every event boundary, stack ends and conservative fallbacks\n",cases,restores,returns);
}
