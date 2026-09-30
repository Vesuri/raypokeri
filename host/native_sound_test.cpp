// Authored synthetic operands and instructions; no ROM data in the fixture.
#include "musashi/m68k.h"
#include "../src/board/Board.h"
#include "../src/native/IrqCache.h"
#include <memory>
#include <array>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <cstdlib>
#undef assert
#define assert(x) do{if(!(x)){std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static std::array<unsigned char,0x180000> memory;
static std::vector<std::pair<unsigned,unsigned>> output;
static constexpr unsigned source=0x110000,frame=0x120000,desc=0x130000,port=0x140000;
static pokeri::Board *model=nullptr;
static pokeri::IrqCache irqCache;
static unsigned modelWrites=0,edgeAt=0,modelIpl=7;
static bool modelDue=false;
static void deviceWrite(unsigned address,unsigned value){
    ++modelWrites;
    // The external edge is delivered before this endpoint, as a board tick
    // would do. Invalidate the peripheral cache at that source mutation.
    if(modelWrites==edgeAt){model->pia[0].edge(0,1,true);irqCache.invalidate();}
    const unsigned bus=0xfb014+address-port;
    irqCache.beforeByte<true>(bus);model->write8(bus,value);
    unsigned irq=irqCache.level(*model,model->video.statusNow());
    assert(irq==model->irq());
    modelDue=model->fault || irq>modelIpl;
}
static unsigned read(unsigned a,unsigned n){assert(a+n<=memory.size());unsigned v=0;while(n--)v=v*256+memory[a++];return v;}
static void write(unsigned a,unsigned n,unsigned v){assert(a+n<=memory.size());while(n){--n;memory[a+n]=v;v>>=8;}}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return read(a,1);}unsigned m68k_read_memory_16(unsigned a){return read(a,2);}unsigned m68k_read_memory_32(unsigned a){return read(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){if(a==port || a==port+2){output.push_back({a,v&255});if(model)deviceWrite(a,v&255);}write(a,1,v);}
void m68k_write_memory_16(unsigned a,unsigned v){write(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){write(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return read(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return read(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return read(a,4);}
void pokeri_exception(unsigned){assert(false && "unexpected CPU exception");}
}
int main(int argc,char**argv){
 assert(argc==3);std::map<std::string,unsigned>s;std::ifstream meta(argv[2]);std::string name;unsigned addr;
 while(meta>>name>>addr)s[name]=addr;
 auto sym=[&](const std::string &n){assert(s.count(n));return s[n];};
 auto set=[&](const char*n,unsigned v,unsigned bytes=4){write(sym(n),bytes,v);};
 FILE*f=fopen(argv[1],"rb");assert(f);auto word=[&](){unsigned v=0;for(unsigned i=0;i<4;++i){int c=fgetc(f);assert(c>=0);v=v*256+c;}return v;};
 unsigned segments=word();while(segments--){unsigned a=word(),n=word();assert(a+n<source);assert(fread(memory.data()+a,1,n,f)==n);}fclose(f);
 m68k_init();unsigned cases=0;
 struct State{unsigned pc,sr,d1,d3,cycles;std::vector<std::pair<unsigned,unsigned>>writes;};
 for(unsigned cpu:{unsigned(M68K_CPU_TYPE_68000),unsigned(M68K_CPU_TYPE_68020)})
 for(unsigned regValue:{0u,0x80u,0xffu})for(unsigned flags=0;flags<32;++flags)for(unsigned value:{0u,2u,0x80u,0x82u,0xfdu,0xffu,0x80000002u,0x7fffff82u}){
  unsigned regs[15];for(unsigned r=0;r<15;++r)regs[r]=0x12345600+r;
  regs[0]=0x13570000|regValue;regs[1]=value;regs[2]=value^0xa55a00ff;regs[11]=port-0x20;
  output.clear();m68k_set_cpu_type(M68K_CPU_TYPE_68000);m68k_set_reg(M68K_REG_SR,0x2700|flags);m68k_set_reg(M68K_REG_SP,frame+0x1000);
  for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),regs[r]);
  m68k_set_reg(M68K_REG_PC,sym("oracle_sound"));std::vector<State>states;unsigned cycles=0;
  while(m68k_get_reg(nullptr,M68K_REG_PC)!=sym("oracle_sound_end")){
   cycles+=m68k_execute(1);states.push_back({m68k_get_reg(nullptr,M68K_REG_PC)-sym("oracle_sound")+source,m68k_get_reg(nullptr,M68K_REG_SR),m68k_get_reg(nullptr,M68K_REG_D1),m68k_get_reg(nullptr,M68K_REG_D3),cycles,output});assert(states.size()<=10);
  }
  assert(states.size()==10 && cycles==100);
  for(unsigned stop=0;stop<=16;++stop)for(unsigned due=0;due<3;++due){
   unsigned bad=stop>10?stop-10:0;
   const unsigned beforeWrite[]={0,3,4,5,8,9,0};
   unsigned expected=bad?beforeWrite[bad]:stop?stop:10;
   regs[11]=port-0x14;output.clear();
   for(unsigned i=0;i<4;++i)write(frame+i*4,4,regs[i<2?i:i+6]);
   write(frame+16,2,0x2700|flags);write(frame+18,4,source);write(frame+22,2,0xa008);
   write(source+2,2,0x14);
   const unsigned offsets[]={0,10,14,18,30,34},r[]={0,1,3,2,1,3};
   for(unsigned i=0;i<6;++i){unsigned d=desc+32*i;write(d,4,source+offsets[i]);write(d+4,4,port+(i==0||i==3?0:2)+(bad && i==bad%6?1:0));write(d+8,2,0x1008|r[i]);write(d+28,4,i<5?d+32:0);}
   set("nativeDiagnostic",0,2);set("nativeShortPending",0,2);set("pendingFrames",0);set("seenFrames",0);set("nativeProfileEnabled",0,2);set("nativeShortNominal",0);set("nativeInstructions",0);set("nativeShortCalls",0);set("nativeClockResumePc",source);
   m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,frame);
   for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),regs[r]);
   m68k_set_reg(M68K_REG_A0,source);m68k_set_reg(M68K_REG_A1,desc);m68k_set_reg(M68K_REG_PC,sym("nativeShortIoGuard"));
   unsigned steps=0,boundaries=0,pc;
   while((pc=m68k_get_reg(nullptr,M68K_REG_PC))!=sym("nativeShortControlPromote") && pc!=sym("nativeShortNoControlDue") && pc!=sym("nativeShortDecline") && steps++<1000){
    if(pc==sym("nativeShortAdmitted")){set("nativeShortNominal",12);set("nativeInstructions",sym("nativeLiveCounterMode")?1:0);m68k_set_reg(M68K_REG_SR,0x2000);m68k_set_reg(M68K_REG_PC,sym("nativeShortSoundWrite"));continue;}
    if(boundaries<10 && pc==sym("nativeSoundBoundary"+std::to_string(boundaries))){
     const auto &e=states[boundaries++];
     assert(read(frame+18,4)==e.pc && read(frame+16,2)==e.sr);
     assert(read(frame+4,4)==e.d1 && m68k_get_reg(nullptr,M68K_REG_D3)==e.d3);
     assert(output==e.writes && read(sym("nativeShortNominal"),4)==e.cycles);
     assert(read(sym("nativeClockResumePc"),4)==e.pc);
     if(stop==boundaries){if(due==0)set("pendingFrames",1);else if(due==1)set("nativeShortPending",2,2);else break;}
    }
    if(pc==sym("nativeShortIoWriteValue")){
     unsigned sp=m68k_get_reg(nullptr,M68K_REG_SP),address=read(sp+4,4),v=read(sp+8,4);
     assert(address==port || address==port+2);output.push_back({address,v&255});
     m68k_set_reg(M68K_REG_D0,v&255);m68k_set_reg(M68K_REG_D1,0xdeadbeef);m68k_set_reg(M68K_REG_A0,0x12340000);m68k_set_reg(M68K_REG_A1,0x56780000);
     m68k_set_reg(M68K_REG_PC,read(sp,4));m68k_set_reg(M68K_REG_SP,sp+4);continue;
    }
    m68k_execute(1);
   }
   assert(steps<1000 && boundaries==expected);
   if(bad==6)assert(pc==sym("nativeShortDecline") && output.empty() && read(frame+18,4)==source && read(frame+4,4)==regs[1]);
   else if(bad)assert(pc==sym("nativeShortControlPromote"));
   assert(m68k_get_reg(nullptr,M68K_REG_SP)==frame && read(frame+22,2)==0xa008);
   assert(read(frame,4)==regs[0] && read(frame+8,4)==regs[8] && read(frame+12,4)==regs[9]);
   for(unsigned r=2;r<15;++r)if(r!=3 && r!=8 && r!=9)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==regs[r]);
   assert(read(sym("nativeInstructions"),4)==(sym("nativeLiveCounterMode")?boundaries:0));++cases;
  }
 }
 // Real shared device models: compare ordinary CPU writes with the linked
 // fused body. The C endpoint ABI is intercepted, but its Board write and IRQ
 // cache operations execute here; no parallel PIA/AY implementation is used.
 unsigned modelCases=0;
 for(unsigned cpu:{unsigned(M68K_CPU_TYPE_68000),unsigned(M68K_CPU_TYPE_68020)})
 for(unsigned reg=0;reg<16;++reg)for(unsigned ipl:{0u,5u,7u})for(unsigned edge=0;edge<=6;++edge){
  std::vector<unsigned char> reference;std::vector<std::pair<unsigned,unsigned>> writes;
  unsigned expectedPc=0,expectedSr=0,expectedD1=0,expectedD3=0,expectedCycles=0;
  bool expectedFault=false,expectedDue=false;
  for(unsigned fused=0;fused<2;++fused){
   std::unique_ptr<pokeri::Board> board(new pokeri::Board());model=board.get();
   model->pia[0].control[0]=7;model->pia[0].control[1]=4;
   model->pia[0].direction[0]=model->pia[0].direction[1]=255;
   model->pia[0].output[1]=2;
   irqCache.invalidate();modelWrites=0;edgeAt=edge;modelIpl=ipl;modelDue=false;
   output.clear();
   unsigned regs[15];for(unsigned r=0;r<15;++r)regs[r]=0x12345600+r;
   regs[0]=reg;regs[1]=2;regs[2]=0xb5;regs[11]=port-(fused?0x14:0x20);
   m68k_set_cpu_type(fused?cpu:M68K_CPU_TYPE_68000);
   m68k_set_reg(M68K_REG_SR,0x2000|(ipl<<8)|0x15);
   m68k_set_reg(M68K_REG_SP,frame);
   for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),regs[r]);
   unsigned cycles=0,steps=0;
   if(!fused){
    m68k_set_reg(M68K_REG_PC,sym("oracle_sound"));
    while(m68k_get_reg(nullptr,M68K_REG_PC)!=sym("oracle_sound_end") && !modelDue){cycles+=m68k_execute(1);assert(++steps<=10);}
    expectedPc=m68k_get_reg(nullptr,M68K_REG_PC)-sym("oracle_sound")+source;
    expectedSr=m68k_get_reg(nullptr,M68K_REG_SR);expectedD1=m68k_get_reg(nullptr,M68K_REG_D1);expectedD3=m68k_get_reg(nullptr,M68K_REG_D3);expectedCycles=cycles;
   }else{
    for(unsigned i=0;i<4;++i)write(frame+i*4,4,regs[i<2?i:i+6]);
    write(frame+16,2,0x2000|(ipl<<8)|0x15);write(frame+18,4,source);write(source+2,2,0x14);
    const unsigned offsets[]={0,10,14,18,30,34},r[]={0,1,3,2,1,3};
    for(unsigned i=0;i<6;++i){unsigned d=desc+32*i;write(d,4,source+offsets[i]);write(d+4,4,port+(i==0||i==3?0:2));write(d+8,2,0x1008|r[i]);write(d+28,4,i<5?d+32:0);}
    set("nativeDiagnostic",0,2);set("nativeShortPending",0,2);set("pendingFrames",0);set("seenFrames",0);set("nativeProfileEnabled",0,2);set("nativeShortNominal",0);set("nativeInstructions",0);set("nativeClockResumePc",source);
    m68k_set_reg(M68K_REG_A0,source);m68k_set_reg(M68K_REG_A1,desc);m68k_set_reg(M68K_REG_PC,sym("nativeShortIoGuard"));
    unsigned pc;
    while((pc=m68k_get_reg(nullptr,M68K_REG_PC))!=sym("nativeShortControlPromote") && pc!=sym("nativeShortNoControlDue")){
     assert(++steps<1000);
     if(pc==sym("nativeShortAdmitted")){set("nativeShortNominal",12);set("nativeInstructions",sym("nativeLiveCounterMode")?1:0);m68k_set_reg(M68K_REG_SR,0x2000);m68k_set_reg(M68K_REG_PC,sym("nativeShortSoundWrite"));continue;}
     if(pc==sym("nativeShortIoWriteValue")){
      unsigned sp=m68k_get_reg(nullptr,M68K_REG_SP),a=read(sp+4,4),v=read(sp+8,4);
      output.push_back({a,v&255});deviceWrite(a,v&255);set("nativeShortPending",modelDue?2:0,2);
      m68k_set_reg(M68K_REG_D0,v&255);m68k_set_reg(M68K_REG_D1,0xdeadbeef);m68k_set_reg(M68K_REG_A0,0x12340000);m68k_set_reg(M68K_REG_A1,0x56780000);
      m68k_set_reg(M68K_REG_PC,read(sp,4));m68k_set_reg(M68K_REG_SP,sp+4);continue;
     }
     m68k_execute(1);
    }
    assert((pc==sym("nativeShortControlPromote"))==expectedDue);
    assert(read(frame+18,4)==expectedPc && read(frame+16,2)==expectedSr);
    assert(read(frame+4,4)==expectedD1 && m68k_get_reg(nullptr,M68K_REG_D3)==expectedD3);
    assert(read(sym("nativeShortNominal"),4)==expectedCycles);
    assert(output==writes && model->fault==expectedFault && modelDue==expectedDue);
   }
   bool fault=model->fault;model->fault=false; // State rejects fault snapshots.
   pokeri::State state;model->state(state);model->fault=fault;
   if(!fused){reference=std::move(state.bytes);writes=output;expectedFault=fault;expectedDue=modelDue;}
   else assert(state.bytes==reference);
   if(fault)assert(std::string(model->faultReason)=="AY-3-8912 has no register 15");
   model=nullptr;
  }
  ++modelCases;
 }
 printf("PASS: %u real-model sound cases: all AY registers, IRQ at every write, IPL 0/5/7, register-15 fault and full device-state equality\n",modelCases);
 printf("PASS: %u sound-block cases, every original boundary, six write arguments/order, flags, PC, cycles and preserved registers/stack on 68000/68020\n",cases);
}
