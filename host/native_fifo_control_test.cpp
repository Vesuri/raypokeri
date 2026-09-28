// Independent synthetic MOVE sequences; no original ROM data is embedded.
#include "musashi/m68k.h"
#include "../src/board/Hd63484.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
static std::array<unsigned char,1048576> memory;
static constexpr unsigned code=0x60000,frame=0x80000,desc=0x90000,fields=0x91000,port=0xa0000;
static pokeri::Hd63484 *device;
static bool oracle=false;
static unsigned read(unsigned a,unsigned n){assert(a<=memory.size()-n);unsigned v=0;while(n--)v=v*256+memory[a++];return v;}
static void write(unsigned a,unsigned n,unsigned v){assert(a<=memory.size()-n);while(n){--n;memory[a+n]=v;v>>=8;}}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return read(a,1);}unsigned m68k_read_memory_16(unsigned a){return read(a,2);}unsigned m68k_read_memory_32(unsigned a){return read(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){if(oracle && (a==port || a==port+2))device->write8(a-port,v);else write(a,1,v);}
void m68k_write_memory_16(unsigned a,unsigned v){write(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){write(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return read(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return read(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return read(a,4);}
void pokeri_exception(unsigned){assert(false && "unexpected CPU exception");}
}
int main(int argc,char **argv){
 assert(argc==3);std::map<std::string,unsigned>s;std::ifstream meta(argv[2]);std::string name;unsigned address;
 while(meta>>name>>address)s[name]=address;
 auto sym=[&](const char*n){assert(s.count(n));return s[n];};
 auto set=[&](const char*n,unsigned v,unsigned nbytes=4){write(sym(n),nbytes,v);};
 FILE*f=fopen(argv[1],"rb");assert(f);auto word=[&](){unsigned v=0;for(unsigned i=0;i<4;++i){int c=fgetc(f);assert(c>=0);v=v*256+c;}return v;};
 unsigned segments=word();while(segments--){unsigned a=word(),n=word();assert(a+n<code);assert(fread(memory.data()+a,1,n,f)==n);}fclose(f);
 m68k_init();unsigned cases=0;
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})for(unsigned flags=0;flags<32;++flags)
 for(unsigned ipl:{0u,4u,5u,7u})for(unsigned value:{0x80u,0x81u})for(unsigned status:{0u,0x80u})
 for(unsigned phases=0;phases<4;++phases)for(unsigned due=0;due<7;++due)for(unsigned bad=0;bad<7;++bad){
  // Bad cases: first EA, second displacement, third admitted endpoint.
  unsigned offsets[]={0,4,10},lengths[]={4,6,4},values[]={3,value,0};
  for(unsigned n=0;n<3;++n){unsigned p=code+offsets[n];write(p,2,n==1?0x117c:0x10bc);write(p+2,2,values[n]);}
  write(code+8,2,bad==2?4:2);
  unsigned initial[15];for(unsigned r=0;r<15;++r)initial[r]=0x12345000+r;
  initial[8]=bad==4?0:bad==5?port-2:bad==6?0xffffffffu:port+unsigned(bad==1);unsigned sr=0x2000|(ipl<<8)|flags;
  pokeri::Hd63484 reference(false);reference.ar=0x91;(*reference.addressSelector().writePhase)=phases&1;(*reference.addressSelector().readPhase)=phases&2;reference.control[3]=0;reference.status=status;
  device=&reference;oracle=true;
  m68k_set_cpu_type(M68K_CPU_TYPE_68000);m68k_set_reg(M68K_REG_SR,sr);m68k_set_reg(M68K_REG_SP,frame+0x2000);
  for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),initial[r]);
  m68k_set_reg(M68K_REG_PC,code);
  unsigned done=0,cycles=0;
  while(done<3){
   if(((bad==1 || bad>=4) && done==0)||(bad==2 && done==1)||(bad==3 && done==2))break;
   cycles+=m68k_execute(1);++done;
   if((due && (due+1)/2==done)||(reference.irq() && ipl<5))break;
  }
  unsigned expectedPc=m68k_get_reg(nullptr,M68K_REG_PC),expectedSr=m68k_get_reg(nullptr,M68K_REG_SR);
  oracle=false;
  pokeri::Hd63484 actual(false);actual.ar=0x91;(*actual.addressSelector().writePhase)=phases&1;(*actual.addressSelector().readPhase)=phases&2;actual.control[3]=0;actual.status=status;
  write(fields,1,actual.ar);write(fields+1,1,(*actual.addressSelector().writePhase));write(fields+2,1,(*actual.addressSelector().readPhase));
  set("nativeVideoSelector",fields);write(sym("nativeVideoSelector")+4,4,fields+1);write(sym("nativeVideoSelector")+8,4,fields+2);
  for(unsigned n=0;n<3;++n){unsigned d=desc+n*32;write(d,4,code+offsets[n]);write(d+4,4,port+(n==1?2:0)+(n==2&&bad==3?1:0));write(d+8,2,n==1?0x0801:0x0800);write(d+10,2,n==1?16:12);write(d+24,2,lengths[n]);write(d+26,2,2);write(d+28,4,n<2?d+32:0);}
  for(unsigned i=0;i<4;++i)write(frame+i*4,4,initial[i<2?i:i+6]);
  write(frame+16,2,sr);write(frame+18,4,code);write(frame+22,2,0x28);
  set("nativeDiagnostic",0,2);set("pendingFrames",0);set("seenFrames",0);set("nativeShortPending",0,2);set("nativeInstructions",0);set("nativeShortNominal",0);set("nativeShortCalls",0);
  set("nativeFeedInlineCount",0);set("nativeFeedHeaderGrant",0);set("nativeProfileEnabled",0,2);set("nativeClockResumePc",code);
  write(sym("nativeRegisters")+68,2,sr);
  m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,frame);
  for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),initial[r]);
  m68k_set_reg(M68K_REG_A0,code);m68k_set_reg(M68K_REG_A1,desc);m68k_set_reg(M68K_REG_PC,sym("nativeShortVideoGuard"));
  unsigned steps=0,boundaries=0,pc;
  while((pc=m68k_get_reg(nullptr,M68K_REG_PC))!=sym("nativeShortControlPromote") && pc!=sym("nativeShortNoControlDue") && pc!=sym("nativeShortDecline") && steps++<1000){
   if(pc==sym("nativeShortAdmitted")){
    set("nativeInstructions",1);set("nativeShortNominal",12);m68k_set_reg(M68K_REG_SR,0x2000);m68k_set_reg(M68K_REG_PC,sym("nativeShortFifoControl"));continue;
   }
   if(pc==sym("nativeFifoControlBoundary")){
    ++boundaries;if(due && (due+1)/2==boundaries){if(due&1)set("pendingFrames",1);else set("nativeShortPending",2,2);}
   }
   if(pc==sym("nativeShortVideoWriteValue")){
    unsigned sp=m68k_get_reg(nullptr,M68K_REG_SP),v=read(sp+8,4);
    assert(read(sp+4,4)==port+2 && read(sp+12,4)==1);
    actual.ar=read(fields,1);(*actual.addressSelector().writePhase)=read(fields+1,1);(*actual.addressSelector().readPhase)=read(fields+2,1);actual.write8(2,v);
    write(fields,1,actual.ar);write(fields+1,1,(*actual.addressSelector().writePhase));write(fields+2,1,(*actual.addressSelector().readPhase));
    set("nativeShortPending",unsigned(actual.irq())|((actual.irq()&&ipl<5)?2:0),2);
    m68k_set_reg(M68K_REG_D0,v);m68k_set_reg(M68K_REG_D1,0xdeadbeef);m68k_set_reg(M68K_REG_A0,0xabcdef00);m68k_set_reg(M68K_REG_A1,0x76543210);
    m68k_set_reg(M68K_REG_PC,read(sp,4));m68k_set_reg(M68K_REG_SP,sp+4);continue;
   }
   m68k_execute(1);
  }
  if(!(steps<1000 && read(frame+18,4)==expectedPc && read(frame+16,2)==expectedSr)){
   fprintf(stderr,"triplet mismatch cpu=%u flags=%u ipl=%u value=%x status=%x due=%u bad=%u pc=%x/%x sr=%x/%x steps=%u\n",cpu,flags,ipl,value,status,due,bad,read(frame+18,4),expectedPc,read(frame+16,2),expectedSr,steps);return 1;
  }
  assert(read(sym("nativeInstructions"),4)==done && read(sym("nativeShortNominal"),4)==cycles);
  assert(read(sym("nativeClockResumePc"),4)==expectedPc);
  assert(read(fields,1)==reference.ar && bool(read(fields+1,1))==(*reference.addressSelector().writePhase) && bool(read(fields+2,1))==(*reference.addressSelector().readPhase) && actual.control[3]==reference.control[3]);
  assert(m68k_get_reg(nullptr,M68K_REG_SP)==frame && read(frame+22,2)==0x28);
  for(unsigned i=0;i<4;++i)assert(read(frame+i*4,4)==initial[i<2?i:i+6]);
  for(unsigned r=2;r<15;++r)if(r!=8&&r!=9)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==initial[r]);
  ++cases;
 }
 printf("PASS: %u FIFO-control triplets, independent CPU/model, each event boundary, bad EAs, CCR, cycles and register/stack preservation\n",cases);
}
