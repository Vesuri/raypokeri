// Synthetic data only. Verify both the standalone kernel and linked feeder.
#include "musashi/m68k.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <map>
#include <string>
static std::array<unsigned char,0x100000> mem;
static unsigned rd(unsigned a,unsigned n){assert(a+n<=mem.size());unsigned v=0;while(n--)v=v*256+mem[a++];return v;}
static void wr(unsigned a,unsigned n,unsigned v){assert(a+n<=mem.size());while(n--){mem[a+n]=v;v>>=8;}}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return rd(a,1);}unsigned m68k_read_memory_16(unsigned a){return rd(a,2);}unsigned m68k_read_memory_32(unsigned a){return rd(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){wr(a,1,v);}void m68k_write_memory_16(unsigned a,unsigned v){wr(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){wr(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return rd(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return rd(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return rd(a,4);}
void pokeri_exception(unsigned){assert(false && "unexpected exception");}
}
int main(int argc,char **argv){
 assert(argc==2 || argc==3);bool integrated=argc==3;
 std::map<std::string,unsigned> symbols;
 if(integrated){std::ifstream meta(argv[2]);std::string name;unsigned address;while(meta>>name>>address)symbols[name]=address;}
 auto sym=[&](const char *name){assert(symbols.count(name));return symbols[name];};
 std::ifstream in(argv[1],std::ios::binary);assert(in);
 if(!integrated){in.read((char*)mem.data()+0x1000,0x800);assert(in.gcount()>0);}
 else{auto number=[&](){unsigned n=0;for(unsigned i=0;i<4;++i){int c=in.get();assert(c>=0);n=n*256+c;}return n;};
  unsigned segments=number();while(segments--){unsigned a=number(),n=number();assert(a+n<0x70000);in.read((char*)mem.data()+a,n);assert(unsigned(in.gcount())==n);}}
 unsigned entry=integrated?sym("nativeFeedLoopCallModel"):0x1000;
 unsigned fallback=integrated?sym("nativeShortVideoWriteValue"):0;
 const unsigned stop=0x60000;
 m68k_init();unsigned cases=0,parameterCases=0;bool parameters[32]={};
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020})
 for(unsigned stage:{6u,7u,28u,77u,78u})for(unsigned n:{1u,2u,3u,29u,64u,65u})
 for(unsigned prefix:{0u,37u})
 for(int x:{-32768,-80,-78,-3,0,32690,32767})for(int y:{-32768,0,32767})for(unsigned origin:{0u,15u,0xc0ffffffu})
 for(unsigned rectangle:{0u,1u})for(unsigned capture:{0u,1u})for(unsigned mutation=0;mutation<=n+13;++mutation){
  for(unsigned a=0x70000;a<0x75000;++a)mem[a]=0xa5;
  const unsigned g=0x70000,words=0x71000,offsets=0x71400,progress=0x71800,buffer=0x72000,pending=0x73000,param=0x74000,counters=0x74200,state=0x74600;
  unsigned ptrs[]={words,offsets,progress,capture?buffer:0,pending,param,state,state+4,state+8,state+12,state+16,state+17,state+20,state+24,state+25,state+28,counters};
  for(unsigned i=0;i<17;++i)wr(g+i*4,4,ptrs[i]);
  wr(g+68,4,x);wr(g+72,4,y);wr(g+76,4,origin);wr(g+80,4,rectangle);wr(g+84,4,mutation>=n+10?(origin&1):mutation>=n+6);wr(g+88,4,mutation>=n+10);
  wr(state,4,stage);wr(state+4,4,prefix);wr(state+8,4,n-1);wr(state+12,4,n==1?0:n);
  wr(offsets+stage*2,2,prefix);wr(offsets+stage*2+2,2,prefix+n);
  unsigned group=n==1?50:39;
  for(unsigned i=0;i<n;++i){unsigned v=i?0x2345+i:group<<10;wr(words+(prefix+i)*2,2,v);wr(pending+i*2,2,v);}
  unsigned value=rd(words+(prefix+n-1)*2,2);
  if(mutation<n){if(mutation==n-1)value^=1;else wr(pending+mutation*2,2,rd(pending+mutation*2,2)^1);}
  if(mutation==n+1)wr(state+12,4,123);
  unsigned pr=0;
  if(mutation>=n+2 && mutation!=n+3){
   group=(mutation==n+2 || mutation>=n+10)?32:(mutation==n+5 || mutation==n+7)?33:2;
   pr=mutation==n+8?12:mutation==n+9?13:0;
   if(mutation==n+6 && n==2 && stage>6 && stage<78)pr=parameterCases++&31;
   unsigned op=(group<<10)|(group==2?pr:0);
   wr(words+prefix*2,2,op);wr(pending,2,op);if(n==1)value=op;
  }
  if(mutation>=n+10 && n==3){
   wr(pending+2,2,rd(words+(prefix+1)*2,2)+x);
   value=uint16_t(rd(words+(prefix+2)*2,2)+y);
   if(mutation==n+11)wr(pending+2,2,rd(pending+2,2)^1);
   if(mutation==n+12)value^=1;
   if(mutation==n+13)wr(pending,2,rd(pending,2)^1);
  }
  wr(param+36,2,x);wr(param+38,2,y);
  wr(progress+stage*12,2,77);wr(progress+stage*12+2,2,99);
  wr(progress+stage*12+4,4,0x12345678);wr(progress+stage*12+8,4,0x89abcdef);
  wr(counters+group*4,4,0xffffffff);
  auto before=mem;
  bool control=mutation==n+6 ? (n==2 && pr!=12 && pr!=13) : mutation==n+7 && n==3;
  bool absolute=mutation==n+10 && n==3 &&
   int16_t(rd(pending+2,2))-int16_t(rd(words+(prefix+1)*2,2))==x &&
   int16_t(value)-int16_t(rd(words+(prefix+2)*2,2))==y;
  bool accepted=n<=64 && stage>6 && stage<78 && (mutation==n || mutation==n+3 || control || absolute);
  if(accepted){
   wr(pending+(n-1)*2,2,value);wr(state+16,1,value>>8);
   if(capture)for(unsigned i=0;i<n;++i)wr(buffer+(prefix+i)*2,2,rd(pending+i*2,2));
   wr(state+4,4,prefix+n);wr(state,4,stage+1);
   if(group==2){wr(param+pr*2,2,value);parameters[pr]=true;}
   else {
   int px=int16_t(x+77),py=int16_t(y+99);
   if(group==32){px=int16_t(rd(pending+2,2));py=int16_t(value);}
   if(group==33){px=int16_t(x+int16_t(rd(pending+2,2)));py=int16_t(y+int16_t(value));}
   int dot=px+int((origin&15)>>2);
   int word=dot>=0?dot/4:-int((unsigned(-dot)+3)/4);
   unsigned address=((origin>>4)+unsigned(word)-unsigned(py*152))&0xfffff;
   wr(param+36,2,px);wr(param+38,2,py);wr(param+32,2,(origin>>16&0xc000)|(address>>12));wr(param+34,2,(address<<4)|((unsigned(dot)&3)<<2));
   wr(state+20,4,(group==32 || group==33)?0:rectangle?0x89abcdef:0x12345678);wr(state+24,1,0);
   if(group!=32 && group!=33){wr(state+25,1,0);wr(state+28,4,0);}
   }
   wr(counters+group*4,4,0);wr(state+8,4,0);wr(state+12,4,0);wr(state+17,1,rd(state+17,1)|0x20);
  }
  auto expected=mem;mem=before;
  unsigned regs[16];for(unsigned i=0;i<16;++i)regs[i]=0x34560000+i;
  regs[1]=value;regs[8]=g;regs[15]=0xffe00;
  if(integrated){
   for(unsigned i=0;i<92;++i)mem[sym("nativeRasterGrant")+i]=mem[g+i];
   wr(sym("nativeRasterGrantActive"),4,1);wr(sym("nativeRasterHits"),4,0);
   wr(sym("nativeFeedHeaderGrant"),4,0);wr(sym("nativeCachedVideoStatus"),1,0xc6);
   regs[9]=0x76000;wr(regs[9]+4,4,0xf6002);
  }
  m68k_set_cpu_type(cpu);m68k_set_reg(M68K_REG_SR,0x2700);
  for(unsigned i=0;i<16;++i)m68k_set_reg(m68k_register_t(M68K_REG_D0+i),regs[i]);
  wr(regs[15],4,stop);m68k_set_reg(M68K_REG_PC,entry);
  unsigned steps=0,calls=0;
  while(m68k_get_reg(nullptr,M68K_REG_PC)!=stop && ++steps<2000){
   if(integrated && m68k_get_reg(nullptr,M68K_REG_PC)==fallback){
    unsigned sp=m68k_get_reg(nullptr,M68K_REG_SP);
    assert(rd(sp+4,4)==0xf6002 && rd(sp+8,4)==value && rd(sp+12,4)==7);
    ++calls;m68k_set_reg(M68K_REG_D0,value);m68k_set_reg(M68K_REG_PC,rd(sp,4));m68k_set_reg(M68K_REG_SP,sp+4);
   }else m68k_execute(1);
  }
  assert(steps<2000);assert(m68k_get_reg(nullptr,M68K_REG_D0)==(integrated?value:unsigned(accepted)));
  if(integrated){
   assert(calls==unsigned(!accepted));assert(rd(sym("nativeRasterHits"),4)==unsigned(accepted));
   assert(rd(sym("nativeCachedVideoStatus"),1)==(accepted?0xe6u:0xc6u));
   assert(rd(sym("nativeFeedHeaderGrant"),4)==unsigned(accepted));
   if(accepted){assert(rd(sym("nativeFeedInlineWord"),4)==pending);
    assert(rd(sym("nativeFeedInlinePending"),4)==state+8);
    assert(rd(sym("nativeFeedInlineHigh"),4)==state+16);
    assert(rd(sym("nativeFeedInlineLength"),4)==state+12);}
  }
  for(unsigned i=1;i<15;++i)if(i!=8)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+i))==regs[i]);
  assert(m68k_get_reg(nullptr,M68K_REG_SP)==regs[15]+4);
  for(unsigned a=0x70000;a<0x75000;++a)if(mem[a]!=expected[a]){std::fprintf(stderr,"case %u address %x expected %x got %x\n",cases,a,expected[a],mem[a]);return 1;}
  ++cases;
 }
 for(unsigned pr=0;pr<32;++pr)assert(parameters[pr]==(pr!=12 && pr!=13));
 std::printf("PASS: %u raster kernel/wrapper cases, CPU 68000/68020, exact state and preserved registers\n",cases);
}
