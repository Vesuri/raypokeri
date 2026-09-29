// Independent synthetic byte-state oracle for the linked native implementation.
#include "musashi/m68k.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <map>
#include <string>
#include <vector>
static std::array<unsigned char,0x100000> mem;
static unsigned rd(unsigned a,unsigned n){assert(a+n<=mem.size());unsigned v=0;while(n--)v=v*256+mem[a++];return v;}
static void wr(unsigned a,unsigned n,unsigned v){assert(a+n<=mem.size());while(n--){mem[a+n]=v;v>>=8;}}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return rd(a,1);}unsigned m68k_read_memory_16(unsigned a){return rd(a,2);}unsigned m68k_read_memory_32(unsigned a){return rd(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){if(a==0xbfee01){assert(v==0x11);return;}wr(a,1,v);}void m68k_write_memory_16(unsigned a,unsigned v){wr(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){wr(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return rd(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return rd(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return rd(a,4);}
void pokeri_exception(unsigned){assert(false && "unexpected exception");}
}
int main(int argc,char **argv){
 assert(argc==3);std::map<std::string,unsigned>s;std::ifstream meta(argv[2]);std::string name;unsigned address;
 while(meta>>name>>address)s[name]=address;
 auto sym=[&](const char*n){assert(s.count(n));return s[n];};
 std::ifstream in(argv[1],std::ios::binary);auto num=[&](){unsigned v=0;for(unsigned i=0;i<4;++i){int c=in.get();assert(c>=0);v=v*256+c;}return v;};
 unsigned segments=num();while(segments--){unsigned a=num(),n=num();assert(a+n<0x7f000);in.read((char*)mem.data()+a,n);assert(unsigned(in.gcount())==n);}
 const unsigned stop=0x7f000,sp=0xff000,batch=0x80000,g=batch+8,words=0x82000,offsets=0x83000,progress=0x84000,pending=0x85000,param=0x86000,counters=0x86200,state=0x86400;
 const unsigned groups[]={2,32,33,39,42,43,49,50,2,33},lengths[]={2,3,3,6,2,4,3,1,2,3};
 m68k_init();unsigned cases=0;
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})
 for(int x:{-32768,-3,0,32760,32767})for(int y:{-32768,-1,0,32767})
 for(unsigned origin:{0u,15u,0xc0ffffffu})for(unsigned rectangle:{0u,1u})
 for(unsigned used:{7u,0xfffffff0u})for(unsigned first:{7u,10u})for(unsigned initial:{0u,1u})for(unsigned cut=initial;cut<=29;++cut){
  for(unsigned a=batch;a<0x86500;++a)mem[a]=0xa5;
  unsigned off=0;
  for(unsigned stage=0;stage<79;++stage){
   unsigned index=stage>=7&&stage<17?stage-7:7,group=groups[index],n=lengths[index];
   wr(offsets+stage*2,2,off);
   for(unsigned i=0;i<n;++i){unsigned v=i?unsigned(int16_t(i==1?-7:13)):(group<<10)|(group==2?5:0);if(group==39&&i==1)v=2;
    wr(words+off*2,2,v);if(group==32&&i)v=uint16_t(v+(i==1?x:y));wr(batch+116+off*2,2,v);++off;}
   wr(progress+stage*12,2,int16_t(stage*7-90));wr(progress+stage*12+2,2,int16_t(71-stage*3));
   wr(progress+stage*12+4,4,0x12340000+stage);wr(progress+stage*12+8,4,0x23450000+stage);
  }
  wr(offsets+158,2,off);unsigned begin=rd(offsets+first*2,2),end=begin+cut;
  unsigned ptrs[]={words,offsets,progress,0,pending,param,state,state+4,state+8,state+12,state+16,state+17,state+20,state+24,state+25,state+28,counters};
  for(unsigned i=0;i<17;++i)wr(g+i*4,4,ptrs[i]);
  wr(g+68,4,x);wr(g+72,4,y);wr(g+76,4,origin);wr(g+80,4,rectangle);wr(g+84,4,1);wr(g+88,4,1);
  wr(batch,4,batch+116+end*2);wr(batch+4,4,batch+116+(begin+29)*2);wr(batch+716,4,first);wr(batch+720,4,first);wr(batch+724,4,0);
  wr(state,4,first);wr(state+4,4,used);wr(state+8,4,initial);wr(state+12,4,initial?unsigned(first==10?-2:2):0);wr(state+17,1,0x83);
  if(initial){wr(pending,2,rd(batch+116+begin*2,2));wr(state+16,1,rd(batch+116+begin*2,2)>>8);}
  wr(param+36,2,x);wr(param+38,2,y);for(unsigned group=0;group<64;++group)wr(counters+group*4,4,0xffffffffu);
  auto before=mem;
  unsigned stage=first,pos=begin;
  while(pos<end){
   unsigned group=rd(words+pos*2,2)>>10,n=rd(offsets+(stage+1)*2,2)-pos;
   if(n>end-pos)break;
   wr(counters+group*4,4,rd(counters+group*4,4)+1);
   if(group==2)wr(param+10,2,rd(words+(pos+1)*2,2));
   else {
    int px=int16_t(x+int16_t(rd(progress+stage*12,2))),py=int16_t(y+int16_t(rd(progress+stage*12+2,2)));
    if(group==32){px=int16_t(rd(batch+116+(pos+1)*2,2));py=int16_t(rd(batch+116+(pos+2)*2,2));}
    if(group==33){px=int16_t(int16_t(rd(param+36,2))+int16_t(rd(words+(pos+1)*2,2)));py=int16_t(int16_t(rd(param+38,2))+int16_t(rd(words+(pos+2)*2,2)));}
    int dot=px+int((origin&15)>>2),w=dot>=0?dot/4:-int((unsigned(-dot)+3)/4);
    unsigned a=((origin>>4)+unsigned(w)-unsigned(py*152))&0xfffff;
    wr(param+32,2,(origin>>16&0xc000)|(a>>12));wr(param+34,2,(a<<4)|((unsigned(dot)&3)<<2));
    wr(param+36,2,px);wr(param+38,2,py);wr(state+20,4,group==32||group==33?0:rd(progress+stage*12+(rectangle?8:4),4));wr(state+24,1,0);
    if(group!=32&&group!=33){wr(state+25,1,0);wr(state+28,4,0);}
   }
   pos+=n;++stage;wr(state+17,1,rd(state+17,1)|0x20);
  }
  if(cut){
   unsigned count=end-pos;wr(state,4,stage);wr(state+4,4,used+pos-begin);wr(state+8,4,count);
   unsigned group=rd(words+pos*2,2)>>10;
   int len=count?(group==39?(count==1?-2:6):int(rd(offsets+(stage+1)*2,2)-pos)):0;
   wr(state+12,4,unsigned(len));wr(state+16,1,rd(batch+116+(end-1)*2,2)>>8);
   for(unsigned i=0;i<count;++i)wr(pending+i*2,2,rd(batch+116+(pos+i)*2,2));
  }
  auto expected=mem;mem=before;
  unsigned regs[16];for(unsigned i=0;i<16;++i)regs[i]=0x34560000+i;regs[15]=sp;
  m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);for(unsigned i=0;i<16;++i)m68k_set_reg(m68k_register_t(M68K_REG_D0+i),regs[i]);
  wr(sp,4,stop);wr(sp+4,4,batch);m68k_set_reg(M68K_REG_PC,sym("_ZN6pokeri11CachedBatch11materializeEv"));
  unsigned steps=0;while(m68k_get_reg(nullptr,M68K_REG_PC)!=stop&&++steps<10000)m68k_execute(1);assert(steps<10000);
  assert(rd(batch,4)==0&&rd(batch+4,4)==0);
  for(unsigned a=pending;a<0x86500;++a)if(mem[a]!=expected[a]){std::fprintf(stderr,"materialize case=%u cut=%u address=%x expected=%x got=%x\n",cases,cut,a,expected[a],mem[a]);return 1;}
  for(unsigned i=2;i<15;++i)if(i!=8&&i!=9)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+i))==regs[i]);
  assert(m68k_get_reg(nullptr,M68K_REG_SP)==sp+4);++cases;
 }
 unsigned acceptance=0;
 const unsigned liveBatch=sym("nativeBatch"),finish=sym("nativeBatchFinish"),endpoint=sym("nativeShortVideoWriteValue"),entry=sym("nativeFeedAcceptWord"),desc=0x90000,expectedWord=0x88000;
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})for(unsigned value=0;value<65536;++value)for(unsigned mode=0;mode<4;++mode){
  wr(liveBatch,4,mode?expectedWord:0);wr(liveBatch+4,4,expectedWord+(mode==3?0:2));wr(expectedWord,2,value^(mode==2));
  wr(sym("nativeFeedInlineCount"),4,0);wr(sym("nativeFeedHeaderGrant"),4,0);wr(sym("nativeRasterGrantActive"),4,0);wr(desc+4,4,0xf6002);
  unsigned regs[16];for(unsigned i=0;i<16;++i)regs[i]=0x34560000+i;regs[1]=value;regs[9]=desc;regs[15]=sp;
  m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2000);for(unsigned i=0;i<16;++i)m68k_set_reg(m68k_register_t(M68K_REG_D0+i),regs[i]);
  wr(sp,4,stop);m68k_set_reg(M68K_REG_PC,entry);
  unsigned steps=0,finishes=0,calls=0;
  while(m68k_get_reg(nullptr,M68K_REG_PC)!=stop&&++steps<500){
   unsigned pc=m68k_get_reg(nullptr,M68K_REG_PC);
   if(pc==finish || pc==endpoint){
    unsigned stack=m68k_get_reg(nullptr,M68K_REG_SP),result=0;
    if(pc==finish){assert(mode>=2 && !calls);++finishes;wr(liveBatch,4,0);wr(liveBatch+4,4,0);}
    else {assert(mode!=1 && finishes==unsigned(mode>=2));assert(rd(stack+4,4)==0xf6002 && rd(stack+8,4)==value && rd(stack+12,4)==7);++calls;result=value;}
    // Deliberate ABI clobbers prove the flush wrapper preserves its live word
    // and descriptor until the ordinary endpoint receives them.
    m68k_set_reg(M68K_REG_D0,result);m68k_set_reg(M68K_REG_D1,0xdeadbeef);m68k_set_reg(M68K_REG_A0,0xabcdef00);m68k_set_reg(M68K_REG_A1,0x76543210);
    m68k_set_reg(M68K_REG_PC,rd(stack,4));m68k_set_reg(M68K_REG_SP,stack+4);
   }else m68k_execute(1);
  }
  assert(steps<500 && calls==unsigned(mode!=1) && finishes==unsigned(mode>=2));
  assert(m68k_get_reg(nullptr,M68K_REG_D0)==value && m68k_get_reg(nullptr,M68K_REG_A1)==desc && m68k_get_reg(nullptr,M68K_REG_SP)==sp+4);
  assert(rd(liveBatch,4)==(mode==1?expectedWord+2:0));
  for(unsigned i=2;i<15;++i)if(i!=8)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+i))==regs[i]);
  ++acceptance;
 }
 unsigned exits=0;
 const char *exitNames[]={"nativeRegisterFeedStore","nativeShortNoControlDue","nativeShortControlPromote"};
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})for(unsigned mode=0;mode<3;++mode)for(unsigned active=0;active<2;++active)for(unsigned flags=0;flags<32;++flags){
  const unsigned saved=0x91000;
  unsigned regs[16];for(unsigned i=0;i<16;++i)regs[i]=0x34560000+i;regs[15]=sp;
  regs[10]=saved;regs[11]=0x88008;regs[4]=0x2500|flags;regs[5]=0x98764;
  wr(liveBatch,4,active?expectedWord:0);wr(liveBatch+4,4,expectedWord+2);wr(sym("nativeDiagnostic"),2,0);
  m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);for(unsigned r=0;r<16;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),regs[r]);
  const unsigned restored[]={0x12345678,0x23456789,0x87654320,0x98765432};
  if(!mode)wr(sp,4,stop);else for(unsigned r=0;r<4;++r)wr(sp+r*4,4,restored[r]);
  unsigned target=mode==0?stop:sym(mode==1?"nativeShortReturn":"nativeShortPromote");
  m68k_set_reg(M68K_REG_PC,sym(exitNames[mode]));unsigned steps=0,finishes=0;
  while(m68k_get_reg(nullptr,M68K_REG_PC)!=target&&++steps<200){
   if(m68k_get_reg(nullptr,M68K_REG_PC)==finish){
    assert(active);++finishes;unsigned stack=m68k_get_reg(nullptr,M68K_REG_SP);
    wr(liveBatch,4,0);wr(liveBatch+4,4,0);
    m68k_set_reg(M68K_REG_D0,0xdead0000);m68k_set_reg(M68K_REG_D1,0xdead0001);m68k_set_reg(M68K_REG_A0,0xdead0008);m68k_set_reg(M68K_REG_A1,0xdead0009);
    m68k_set_reg(M68K_REG_PC,rd(stack,4));m68k_set_reg(M68K_REG_SP,stack+4);
   }else m68k_execute(1);
  }
  assert(steps<200 && finishes==active && !rd(liveBatch,4));
  assert(m68k_get_reg(nullptr,M68K_REG_SP)==sp+(mode?16:4));
  if(!mode){assert(rd(saved+12,4)==regs[11] && rd(saved+16,2)==regs[4] && rd(saved+18,4)==regs[5] && rd(sym("nativeClockResumePc"),4)==regs[5]);}
  for(unsigned r=0;r<15;++r){unsigned want=regs[r];if(mode){if(r<2)want=restored[r];else if(r==8||r==9)want=restored[r-6];}assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==want);}
  ++exits;
 }
 std::printf("PASS: %u batch return/promotion/store exits, materialization before guest transfer with exact registers/frame\n",exits);
 std::printf("PASS: %u active/inactive/limit/mismatch batch acceptances; all words, 68000/68020, flush before fallback and ABI preservation\n",acceptance);
 std::printf("PASS: %u linked batch materializations, CPUs 68000/68020, signed/wrapped CP/DP, partial lengths/latch, 32-bit count wrap and C ABI\n",cases);
}
