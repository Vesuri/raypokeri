// Actual linked wrapper/lookup code; clock C endpoints are explicit test doubles.
#include "musashi/m68k.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <vector>
static std::array<unsigned char,0x1000000> mem;
static std::map<std::string,unsigned> sym;
static bool deliverPorts=false;
static unsigned irqEntries=0,lineEntries=0,portsAcks=0;
static unsigned clockCalls=0,pauseCalls=0,stopWrites=0,portsWrites=0;
static unsigned read(unsigned a,unsigned n){assert(a+n<=mem.size());unsigned v=0;while(n--)v=v*256+mem[a++];return v;}
static void write(unsigned a,unsigned n,unsigned v){assert(a+n<=mem.size());if(a==0xbfee01){assert(n==1&&v==0);++stopWrites;}if(a==0xdff09c){assert(n==2 && (v==0x8008 || (deliverPorts && v==8)));if(v==0x8008){++portsWrites;if(deliverPorts)m68k_set_irq(2);}else{++portsAcks;m68k_set_irq(0);}}else assert(a<0xdff000 || a>0xdfffff);while(n){--n;mem[a+n]=v;v>>=8;}}
extern "C" {
unsigned pokeri_service_cpu_type(void);
unsigned m68k_read_memory_8(unsigned a){return read(a,1);}unsigned m68k_read_memory_16(unsigned a){return read(a,2);}unsigned m68k_read_memory_32(unsigned a){return read(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){write(a,1,v);}void m68k_write_memory_16(unsigned a,unsigned v){write(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){write(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return read(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return read(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return read(a,4);}
void pokeri_exception(unsigned v){if(deliverPorts && v==26){++irqEntries;return;}if(deliverPorts && v==10){++lineEntries;return;}std::fprintf(stderr,"Unexpected CPU exception %u\n",v);std::exit(1);}
}
static unsigned reg(unsigned r){return 0x12340000+r*0x01010101;}
static void setup(unsigned pc,unsigned sp){
 m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,sp);m68k_set_reg(M68K_REG_PC,pc);
 for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),reg(r));
 clockCalls=pauseCalls=stopWrites=portsWrites=0;
}
static unsigned run(unsigned end1,unsigned end2=0){
 for(unsigned step=0;step<120;++step){
  unsigned pc=m68k_get_reg(nullptr,M68K_REG_PC);
  if(pc==end1 || (end2&&pc==end2))return pc;
  if(pc==sym.at("nativeClockEnter") || pc==sym.at("nativeClockPauseInterrupt")){
   if(pc==sym.at("nativeClockEnter")){assert(!pauseCalls);++clockCalls;}else {assert(clockCalls==1);++pauseCalls;}
   unsigned sp=m68k_get_reg(nullptr,M68K_REG_SP);
   m68k_set_reg(M68K_REG_PC,read(sp,4));m68k_set_reg(M68K_REG_SP,sp+4);
   for(unsigned r:{0u,1u,8u,9u})m68k_set_reg(m68k_register_t(M68K_REG_D0+r),0xbad00000+r);
  }else m68k_execute(1);
 }
 assert(false&&"wrapper failed to reach boundary");return 0;
}
int main(int argc,char**argv){
 assert(argc==3);std::ifstream image(argv[1],std::ios::binary),symbols(argv[2]);assert(image&&symbols);
 image.read((char*)mem.data(),0x100000);assert(image.gcount()>0);
 std::string name;unsigned address;while(symbols>>name>>std::hex>>address)sym[name]=address;
 unsigned state=sym.at("nativeServiceRedirectState"),stub=sym.at("nativeServiceOpcode");
 const unsigned frame=0x110000,sentinel=0x120000,guest=0x130000;
 m68k_init();unsigned cases=0;
 write(sym.at("nativeClockEnabled"),2,1);write(sym.at("nativeProfileEnabled"),2,0);
 for(unsigned cpu:{M68K_CPU_TYPE_68000,M68K_CPU_TYPE_68020,M68K_CPU_TYPE_68030,M68K_CPU_TYPE_68040}){
  m68k_set_cpu_type(cpu);assert(pokeri_service_cpu_type()==cpu);
  // The real return helper only requests PORTS at a user return, with no
  // CIA reads/acks or guest/register/frame mutations. Calibration defers it.
  for(unsigned ccr=0;ccr<32;++ccr)for(unsigned flags=0;flags<32;++flags){
   bool pending=flags&1,enabled=flags&2,calibrating=flags&4,supervisor=flags&8,ports=flags&16;
   write(sym.at("nativeServiceRequestPending"),2,pending);
   write(sym.at("nativeServiceRedirectEnabled"),2,enabled);
   write(sym.at("nativeClockCalibrating"),2,calibrating);
   mem[0xdff01d]=ports?8:0;
   unsigned sp=frame-4;write(sp,4,sentinel);write(frame,2,ccr|(supervisor?0x2000:0));
   write(frame+2,4,guest);write(frame+6,2,0xdead);
   setup(sym.at("nativeServiceRequest"),sp);
   bool request=pending && enabled && !calibrating && !supervisor;
   assert(run(sentinel,sym.at("nativeExit"))==(request&&!ports?sym.at("nativeExit"):sentinel));
   if(request && !ports){assert(read(sym.at("nativeStatus"),4)==0xdead);assert(read(sym.at("nativeError"),4));}
   else{
    assert(m68k_get_reg(nullptr,M68K_REG_PC)==sentinel);
    assert(m68k_get_reg(nullptr,M68K_REG_SP)==frame);
    for(unsigned r=0;r<15;++r)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==reg(r));
    assert(read(sym.at("nativeServiceRequestPending"),2)==unsigned(pending&&!request));
   }
   assert(portsWrites==unsigned(request&&ports));
   assert(!clockCalls && !pauseCalls && !stopWrites);
   assert(read(frame,2)==(ccr|(supervisor?0x2000:0)) && read(frame+2,4)==guest && read(frame+6,2)==0xdead);
   ++cases;
  }
  for(unsigned level:{2u,3u,4u,6u})for(unsigned sr=0;sr<65536;++sr)for(unsigned mode=0;mode<3;++mode){
   write(sym.at("nativeServiceRedirectEnabled"),2,mode!=0);
   write(sym.at("nativeClockCalibrating"),2,mode==2);
   write(sym.at("nativeOldLevel"+std::to_string(level)),4,sentinel);
   for(unsigned i=0;i<96;++i)mem[frame+i]=(sr+13*i)&255;
   write(frame,2,sr);write(frame+2,4,guest);
   std::array<unsigned char,96> expected;for(unsigned i=0;i<96;++i)expected[i]=mem[frame+i];
   write(state,4,stub);write(state+4,4,0x76543210);write(state+8,2,0);
   setup(sym.at("nativeLevel"+std::to_string(level)),frame);
   assert(run(sentinel,sym.at("nativeFault"))==sentinel);
   unsigned target=guest,wantSr=sr;
   if(!(sr&0x2000)){
    if(mode==1){target=stub;wantSr&=0x7fff;assert(read(state+4,4)==guest && read(state+8,2)==1);}
    else if(mode==0)wantSr|=0x8000;
   }
   expected[0]=wantSr>>8;expected[1]=wantSr;
   for(unsigned i=0;i<4;++i)expected[2+i]=target>>(24-8*i);
   for(unsigned i=0;i<96;++i)assert(mem[frame+i]==expected[i]);
   if((sr&0x2000)||mode!=1)assert(read(state+4,4)==0x76543210 && read(state+8,2)==0);
   assert(clockCalls==unsigned(!(sr&0x2000)) && pauseCalls==clockCalls && stopWrites==clockCalls);
   assert(m68k_get_reg(nullptr,M68K_REG_SP)==frame);
   for(unsigned r=0;r<15;++r)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==reg(r));
   ++cases;
  }
  // Enter the actual ordinary Line-A lookup, not just its consume helper.
  write(sym.at("nativeClockCalibrating"),2,0);write(sym.at("nativeShortEnabled"),2,1);
  write(sym.at("nativeDiagnostic"),2,0);write(sym.at("nativeShortCount"),2,1);write(stub,2,0xa000);
  unsigned d=sym.at("nativeShortStatus");for(unsigned i=0;i<32;++i)mem[d+i]=0;
  write(d,4,stub);write(d+16,4,sym.at("nativeServiceDescriptor"));
  // Execute a real software IRQ, chained acknowledgement/RTE and Line-A
  // service. The authored old-handler double only acknowledges PORTS; this
  // proves delivery/return, not the full Exec CIA/keyboard implementation.
  for(unsigned ccr=0;ccr<32;++ccr){
   const unsigned old=sentinel+0x100;
   write(old,2,0x33fc);write(old+2,2,8);write(old+4,4,0xdff09c);write(old+8,2,0x4e73);
   write(sentinel,2,0x4e73);write(26*4,4,sym.at("nativeLevel2"));write(10*4,4,sym.at("nativeLineA"));
   write(sym.at("nativeOldLevel2"),4,old);
   write(sym.at("nativeServiceRequestPending"),2,1);write(sym.at("nativeServiceRedirectEnabled"),2,1);
   write(state,4,stub);write(state+4,4,0);write(state+8,2,0);
   mem[0xdff01d]=8;write(frame-4,4,sentinel);write(frame,2,ccr);write(frame+2,4,guest);write(frame+6,2,0);
   m68k_set_reg(M68K_REG_VBR,0);m68k_set_reg(M68K_REG_USP,0x140000);
   setup(sym.at("nativeServiceRequest"),frame-4);
   deliverPorts=true;irqEntries=lineEntries=portsAcks=0;
   assert(run(sym.at("nativeSave"),sym.at("nativeFault"))==sym.at("nativeSave"));
   deliverPorts=false;m68k_set_irq(0);
   assert(irqEntries==1 && lineEntries==1 && portsWrites==1 && portsAcks==1);
   assert(!read(sym.at("nativeServiceRequestPending"),2) && !read(state+8,2));
   assert(read(frame,2)==ccr && read(frame+2,4)==guest);
   for(unsigned r=0;r<15;++r)assert(read(sym.at("nativeRegisters")+4*r,4)==reg(r));
   assert(m68k_get_reg(nullptr,M68K_REG_D0)==11);
   ++cases;
  }
  for(unsigned ccr=0;ccr<32;++ccr)for(unsigned mode=0;mode<5;++mode){
   unsigned faultPc=mode==3?guest:stub;
   write(guest,2,0xa000);write(stub,2,mode==4?0xa001:0xa000);
   write(frame,2,ccr);write(frame+2,4,faultPc);write(frame+6,2,0xa5a5);
   write(state,4,stub);write(state+4,4,guest);write(state+8,2,mode!=1);
   write(sym.at("nativeServiceRedirectEnabled"),2,mode!=2);
   setup(sym.at("nativeLineA"),frame);
   unsigned result=run(sym.at("nativeSave"),sym.at("nativeFault"));
   assert(result==sym.at(mode==1||mode==2?"nativeFault":"nativeSave"));
   assert(read(frame,2)==ccr && read(frame+6,2)==0xa5a5);
   if(mode==0){
    assert(read(frame+2,4)==guest && !read(state+8,2));
    assert(m68k_get_reg(nullptr,M68K_REG_D0)==11);
    for(unsigned r=0;r<15;++r)assert(read(sym.at("nativeRegisters")+4*r,4)==reg(r));
   }else {
    assert(read(frame+2,4)==faultPc);
    if(mode>=3){
     assert(read(state+8,2)==1 && read(state+4,4)==guest);
     assert(m68k_get_reg(nullptr,M68K_REG_D0)==10);
     for(unsigned r=0;r<15;++r)assert(read(sym.at("nativeRegisters")+4*r,4)==reg(r));
    }
   }
   assert(m68k_get_reg(nullptr,M68K_REG_SP)==frame && clockCalls==unsigned(mode>=3) && !pauseCalls);
   ++cases;
  }
 }
 printf("PASS: %u linked IRQ-wrapper/Line-A-lookup cases: all SRs, live/trace/calibration policies, clock ABI clobbers, saved frames/registers, missing/disabled slot faults and non-stub lookup rejection\n",cases);
}
