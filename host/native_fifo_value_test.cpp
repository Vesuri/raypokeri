// Synthetic inputs only. Executes the actual compiled C endpoint and PIA IRQ
// routine; only the general fallback is stubbed and checked for exact arguments.
#include "musashi/m68k.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <map>
#include <string>
#include <vector>
static std::array<unsigned char,0x300000> mem;
static bool watching=false;
static std::vector<unsigned> stores;
static unsigned rd(unsigned a,unsigned n){assert(a+n<=mem.size());unsigned v=0;while(n--)v=v*256+mem[a++];return v;}
static void wr(unsigned a,unsigned n,unsigned v){assert(a+n<=mem.size());if(watching)for(unsigned i=0;i<n;++i)stores.push_back(a+i);while(n--){mem[a+n]=v;v>>=8;}}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return rd(a,1);}unsigned m68k_read_memory_16(unsigned a){return rd(a,2);}unsigned m68k_read_memory_32(unsigned a){return rd(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){wr(a,1,v);}void m68k_write_memory_16(unsigned a,unsigned v){wr(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){wr(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return rd(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return rd(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return rd(a,4);}
void pokeri_exception(unsigned){assert(false && "unexpected CPU exception");}
}
int main(int argc,char**argv){
 assert(argc==3);std::map<std::string,unsigned>s;std::ifstream meta(argv[2]);std::string name;unsigned address;while(meta>>name>>address)s[name]=address;
 auto sym=[&](const char*n){assert(s.count(n));return s[n];};
 const unsigned board=0x100000,sp=0x2f0000,stop=0xf0000;
 auto field=[&](const char*n){std::string key=std::string("offset_")+n;assert(s.count(key));return board+s[key];};
 std::ifstream in(argv[1],std::ios::binary);auto num=[&](){unsigned v=0;for(unsigned i=0;i<4;++i){int c=in.get();assert(c>=0);v=v*256+c;}return v;};
 unsigned segments=num();while(segments--){unsigned a=num(),n=num();assert(a+n<stop);in.read((char*)mem.data()+a,n);assert(unsigned(in.gcount())==n);}
 wr(sym("_ZL5board"),4,board);wr(sym("_ZL11videoDevice"),4,field("video"));
 const char*grants[]={"nativeFeedInlineCount","nativeFeedHeaderGrant","nativeRasterGrantActive"};
 unsigned cases=0;stores.reserve(64);m68k_init();
 auto run=[&](unsigned value,unsigned status,unsigned context,unsigned p0,unsigned f0,unsigned p1,unsigned f1,unsigned ac,unsigned rx,unsigned event,unsigned reject){
  // No video byte phases, pending words, parameters, or other board state may
  // be written by this endpoint. Trap every store instead of sampling memory.
  wr(field("control"),1,value^0x5a);wr(field("status"),1,status);wr(field("hold"),1,context&1);wr(field("pending"),4,(context&2)?17:0);wr(field("read"),4,(context>>2)&15);
  wr(field("ar"),1,reject==2?0x80:3);wr(field("error"),4,reject==3?0xabcdef:0);wr(field("fault"),1,reject==4);
  wr(field("pia_control0"),1,p0);wr(field("pia_flags0"),1,f0);wr(field("pia_control1"),1,p1);wr(field("pia_flags1"),1,f1);wr(field("serial_control"),1,ac);wr(field("serial_read"),4,rx);
  unsigned ipl=(event>>3)&7;bool ticks=event&1,frameDue=event&2,quit=event&4;
  wr(sym("_ZL10diagnostic"),1,reject==1);wr(sym("_ZL13quitRequested"),1,quit);wr(sym("_ZL9liveTicks"),4,ticks?0xffffffffu:0);wr(sym("pendingFrames"),4,frameDue?0:0xffffffffu);wr(sym("seenFrames"),4,0xffffffffu);
  wr(sym("nativeRegisters")+68,2,0x2000|(ipl<<8)|31);wr(sym("nativeCachedVideoStatus"),1,0x5a);wr(sym("nativeShortPending"),2,0x1234);for(auto n:grants)wr(sym(n),4,0x12345678);
  unsigned expectedStatus=status&0xf0;if(!(context&1))expectedStatus|=3;if(context&2)expectedStatus&=~32u;unsigned reads=(context>>2)&15;if(reads)expectedStatus|=4;if(reads>=8)expectedStatus|=8;
  auto pia=[](unsigned c,unsigned f){return ((f&128)&&(c&1))||((f&64)&&(c&8)&&!(c&32));};
  unsigned irq=pia(p0,f0)||pia(p1,f1)||(expectedStatus&uint8_t(value))||((ac&3)!=3&&((ac&96)==32||((ac&128)&&rx)))?5:0;
  unsigned expectedPending=(ticks||irq?1:0)|((frameDue||quit||irq>ipl)?2:0);
  unsigned regs[16];for(unsigned i=0;i<16;++i)regs[i]=0x34560000+i;regs[15]=sp;
  m68k_set_reg(M68K_REG_SR,0x2700);for(unsigned i=0;i<16;++i)m68k_set_reg(m68k_register_t(M68K_REG_D0+i),regs[i]);
  wr(sp,4,stop);wr(sp+4,4,0x1f6002);wr(sp+8,4,value);wr(sp+12,4,1);m68k_set_reg(M68K_REG_PC,sym("nativeFifoControlValue"));
  unsigned steps=0,fallbacks=0;stores.clear();watching=true;
  while(m68k_get_reg(nullptr,M68K_REG_PC)!=stop&&++steps<1000){
   if(m68k_get_reg(nullptr,M68K_REG_PC)==sym("nativeShortVideoWriteValue")){
    assert(reject);++fallbacks;unsigned stack=m68k_get_reg(nullptr,M68K_REG_SP);assert(rd(stack+4,4)==0x1f6002&&rd(stack+8,4)==value&&rd(stack+12,4)==1);
    m68k_set_reg(M68K_REG_D0,value);m68k_set_reg(M68K_REG_D1,0xdeadbeef);m68k_set_reg(M68K_REG_A0,0xabcdef);m68k_set_reg(M68K_REG_A1,0xfedcba);m68k_set_reg(M68K_REG_PC,rd(stack,4));m68k_set_reg(M68K_REG_SP,stack+4);
   }else m68k_execute(1);
  }
  watching=false;assert(steps<1000&&fallbacks==unsigned(reject!=0));
  assert(rd(field("control"),1)==(reject?uint8_t(value^0x5a):uint8_t(value)));
  assert(rd(sym("nativeCachedVideoStatus"),1)==(reject?0x5a:expectedStatus));assert(rd(sym("nativeShortPending"),2)==(reject?0x1234:expectedPending));for(auto n:grants)assert(rd(sym(n),4)==(reject?0x12345678:0));
  for(unsigned a:stores){
   bool allowed=a>=sp-128&&a<sp+16;
   if(!reject){allowed|=a==field("control")||a==sym("nativeCachedVideoStatus")||(a>=sym("nativeShortPending")&&a<sym("nativeShortPending")+2);for(auto n:grants)allowed|=a>=sym(n)&&a<sym(n)+4;}
   if(!allowed){std::fprintf(stderr,"unexpected store %x case %u\n",a,cases);return false;}
  }
  assert(m68k_get_reg(nullptr,M68K_REG_D0)==value&&m68k_get_reg(nullptr,M68K_REG_SP)==sp+4);
  for(unsigned i=2;i<15;++i)if(i!=8&&i!=9)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+i))==regs[i]);++cases;return true;
 };
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020}){
  m68k_set_cpu_type(cpu);
  for(unsigned value=0;value<256;++value)for(unsigned status=0;status<256;++status)for(unsigned ctx:{0u,1u,2u,3u,4u,28u,32u,60u})assert(run(value,status,ctx,0,0,0,0,3,0,(value+status+ctx)&63,0));
  for(unsigned c=0;c<256;++c)for(unsigned f=0;f<256;++f)for(unsigned side=0;side<2;++side)assert(run(0,0,0,side?0:c,side?0:f,side?c:0,side?f:0,3,0,(c+f)&63,0));
  for(unsigned c=0;c<256;++c)for(unsigned n:{0u,1u,9u})for(unsigned event=0;event<64;++event)assert(run(0,0,0,0,0,0,0,c,n,event,0));
  for(unsigned reject=1;reject<=4;++reject)for(unsigned value=0;value<256;++value)assert(run(0xbeef0000u|value,0xff,63,255,255,255,255,255,9,value&63,reject));
 }
 std::printf("PASS: %u actual FIFO service CPU cases; video/PIA/serial sources, pending events, fallback, all stores and C ABI on 68000/68020\n",cases);
}
