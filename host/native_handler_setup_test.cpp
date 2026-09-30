// Independently assembled queue-setup oracle against linked native assembly.
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
static constexpr unsigned frame=0x120000,desc=0x140000,port=0x180000,fields=0x190000,selectFields=0x1b0000;
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
 std::map<unsigned,bool> boundaries;
 for(unsigned i=0;i<12;++i)boundaries[sym("nativeSetupBoundary"+std::to_string(i))]=true;
 for(const char*n:{"nativeSetupNonemptyBoundary","nativeSetupWithinBoundary","nativeSetupFeedBoundary"})boundaries[sym(n)]=true;
 const unsigned code=sym("oracle_setup");const bool counts=sym("nativeLiveCounterMode")!=0;
 m68k_init();unsigned cases=0;
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
  std::vector<State> states;unsigned cycles=0;bool rejected=false;
  oracle=true;selector[0]=0x91;selector[1]=selector[2]=1;
  m68k_set_cpu_type(M68K_CPU_TYPE_68000);m68k_set_reg(M68K_REG_SR,0x2700|flags);
  for(unsigned r=0;r<16;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),initial[r]);
  m68k_set_reg(M68K_REG_PC,code);
  for(;;){
   unsigned pc=m68k_get_reg(nullptr,M68K_REG_PC);
   if(pc==sym("oracle_setup_feed") || pc==sym("oracle_setup_empty") || pc==sym("oracle_setup_exit"))break;
   int displacement=0;
   if(pc==code+4)displacement=-30682;
   if(pc==code+8)displacement=-30526;
   if(pc==code+14)displacement=-30678;
   if(pc==code+26)displacement=-30530;
   if(displacement){unsigned ea=initial[14]+displacement;if((ea&1)||ea<ramFirst||ea+4<ea||ea+4>ramLast){rejected=true;break;}}
   cycles+=m68k_execute(1);State st;
   for(unsigned r=0;r<16;++r)st.regs[r]=m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r));
   st.pc=m68k_get_reg(nullptr,M68K_REG_PC);st.sr=m68k_get_reg(nullptr,M68K_REG_SR);st.cycles=cycles;states.push_back(st);require(states.size()<20);
  }
  oracle=false;require(!states.empty());
  for(unsigned event=0;event<=states.size();++event)for(unsigned eventKind=0;eventKind<2;++eventKind){
   set("nativeDiagnostic",0,2);set("pendingFrames",0);set("seenFrames",0);set("nativeShortPending",0,2);
   set("nativeInstructions",counts?1:0);set("nativeShortNominal",12);set("nativeShortCalls",0);set("nativeProfileEnabled",0,2);
   set("nativeRamBegin",ramFirst);set("nativeRamEnd",ramLast);set("nativeHandlerFeed",desc+32);set("nativeHandlerEmpty",desc+64);set("nativeFeedTarget",sym("oracle_setup_exit"));
   set("nativeFeedInlineCount",9);set("nativeFeedHeaderGrant",9);set("nativeRasterGrantActive",9);
   set("nativeVideoSelector",selectFields);wr(sym("nativeVideoSelector")+4,4,selectFields+2);wr(sym("nativeVideoSelector")+8,4,selectFields+3);
   wr(selectFields,1,0x91);wr(selectFields+1,1,0xa5);wr(selectFields+2,1,1);wr(selectFields+3,1,1);wr(selectFields+4,1,0x5a);
   wr(desc,4,code);wr(desc+32,4,sym("oracle_setup_feed"));wr(desc+64,4,sym("oracle_setup_empty"));
   for(unsigned i=0;i<4;++i)wr(frame+i*4,4,initial[i<2?i:i+6]);
   wr(frame+16,2,0x0700|flags);wr(frame+18,4,code);wr(frame+22,2,0x28);
   m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,frame);m68k_set_reg(M68K_REG_USP,initial[15]);
   for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),initial[r]);
   m68k_set_reg(M68K_REG_A1,desc);m68k_set_reg(M68K_REG_D1,0);m68k_set_reg(M68K_REG_PC,sym("nativeShortHandlerSetup"));
   unsigned steps=0,pc,number=0;
   while((pc=m68k_get_reg(nullptr,M68K_REG_PC))!=sym("nativeShortControlPromote") && pc!=sym("nativeShortNoControlDue") && pc!=sym("nativeShortVideoGuard") && pc!=sym("nativeShortStatusGuard") && pc!=sym("nativeShortAddressWrite")){
    require(++steps<1000);
    if(boundaries.count(pc)){++number;if(number==event){if(eventKind)set("pendingFrames",1);else set("nativeShortPending",2,2);}}
    m68k_execute(1);
   }
   if(pc==sym("nativeShortAddressWrite")){
    require(sym("nativeSetupRegisterMode") && bad);
    require(number==0 && rd(frame+18,4)==code && rd(frame+16,2)==(0x700|flags));
    require(rd(sym("nativeShortNominal"),4)==12 && rd(sym("nativeInstructions"),4)==(counts?1:0));
    for(unsigned i=0;i<4;++i)require(rd(frame+i*4,4)==initial[i<2?i:i+6]);
    for(unsigned r=2;r<15;++r)if(r!=8 && r!=9)require(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==initial[r]);
    require(rd(selectFields,1)==0x91 && rd(selectFields+2,2)==0x101);
    require(rd(sym("nativeFeedInlineCount"),4)==9 && rd(sym("nativeFeedHeaderGrant"),4)==9 && rd(sym("nativeRasterGrantActive"),4)==9);
    require(m68k_get_reg(nullptr,M68K_REG_SP)==frame && m68k_get_reg(nullptr,M68K_REG_USP)==initial[15]);
    require(m68k_get_reg(nullptr,M68K_REG_A1)==desc && m68k_get_reg(nullptr,M68K_REG_D1)==0);
    require(rd(frame+22,2)==0x28);++cases;continue;
   }
   const unsigned completed=event?event:states.size();const State &expected=states[completed-1];
   if(rd(frame+18,4)!=expected.pc || (rd(frame+16,2)&31)!=(expected.sr&31) || rd(sym("nativeShortNominal"),4)!=expected.cycles){
    fprintf(stderr,"setup cpu=%u shape=%u flags=%u bad=%u event=%u pc=%x/%x sr=%x/%x cycles=%u/%u\n",cpu,shape,flags,bad,event,rd(frame+18,4),expected.pc,rd(frame+16,2),expected.sr,rd(sym("nativeShortNominal"),4),expected.cycles);return 1;
   }
   require(number==completed);require(rd(sym("nativeInstructions"),4)==(counts?completed:0));
   require((pc==sym("nativeShortControlPromote"))==bool(event||rejected));
   for(unsigned i=0;i<4;++i)require(rd(frame+i*4,4)==expected.regs[i<2?i:i+6]);
   for(unsigned r=2;r<15;++r)if(r!=8 && r!=9)require(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==expected.regs[r]);
   require(rd(selectFields,1)==0 && rd(selectFields+2,2)==0 && rd(selectFields+1,1)==0xa5 && rd(selectFields+4,1)==0x5a);
   require(rd(sym("nativeClockResumePc"),4)==expected.pc && rd(frame+22,2)==0x28 && (rd(frame+16,2)&0xffe0)==0x700);
   require(m68k_get_reg(nullptr,M68K_REG_SP)==frame && m68k_get_reg(nullptr,M68K_REG_USP)==initial[15]);
   require(!rd(sym("nativeFeedInlineCount"),4) && !rd(sym("nativeFeedHeaderGrant"),4) && !rd(sym("nativeRasterGrantActive"),4));
   if(!event && !rejected && pc!=sym("nativeShortNoControlDue")){
    require(m68k_get_reg(nullptr,M68K_REG_A0)==expected.pc);
    require(m68k_get_reg(nullptr,M68K_REG_A1)==desc+(expected.pc==sym("oracle_setup_empty")?64:32));
   }
   ++cases;
  }
 }
 printf("PASS: %u setup bridge cases, both CPUs, all CCRs, empty/wrapped queues, every boundary, signed/wrapped pointer comparisons and rejected RAM spans\n",cases);
}
