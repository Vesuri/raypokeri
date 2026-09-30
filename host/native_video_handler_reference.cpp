// Execute the user's unmodified video handler in Musashi; never embed ROM data.
// This is a whole-handler reference fixture, not a native-fusion proof.
#include "musashi/m68k.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#define check(x) do { if(!(x)){std::fprintf(stderr,"line %d: %s (pc=%x)\n",__LINE__,#x,m68k_get_reg(nullptr,M68K_REG_PC));std::exit(1);} } while(0)
static std::array<unsigned char,0x100000> mem;
static constexpr unsigned first=0x2e26,normalEnd=0x2e8a,errorEnd=0x2ea8;
static constexpr unsigned ram=0x40000,ramEnd=0x80000,fields=0x41000,ring=0x42000;
static constexpr unsigned stack=0x60000,userStack=0x68000,returnedPc=0x70000,trapPc=0x70002;
struct Access {unsigned address,size,value;bool write;};
struct Boundary {unsigned pc,sr,regs[16],cycles;std::vector<Access> accesses;};
static std::vector<Access> accesses,device;
static std::vector<unsigned> words;
static unsigned ready,errorStatus,control,selector,exceptionVector;
static bool running,injecting;
static unsigned irqEntries;
static constexpr unsigned irqStub=0x70010;
static unsigned rawRead(unsigned a,unsigned n){check(a<=mem.size()-n);unsigned v=0;while(n--)v=(v<<8)|mem[a++];return v;}
static void rawWrite(unsigned a,unsigned n,unsigned v){check(a<=mem.size()-n);while(n){--n;mem[a+n]=v;v>>=8;}}
static unsigned read(unsigned a,unsigned n){
 if(running && (a==0xf6000 || a==0xf6002)){
  check(n==1);unsigned v=a==0xf6000?errorStatus|(words.size()<ready?2:0):control;
  Access e={a,n,v,false};accesses.push_back(e);device.push_back(e);return v;
 }
 if(a==46*4 && n==4)return trapPc;
 if(a==31*4 && n==4)return irqStub;
 check(a<=0x40000-n || (a>=ram && a<=ramEnd-n));return rawRead(a,n);
}
static void write(unsigned a,unsigned n,unsigned v){
 check(running);
 Access e={a,n,v,true};accesses.push_back(e);
 if(a==0xf6000){check(n==1);selector=v;device.push_back(e);return;}
 if(a==0xf6002){check(n==1 || n==2);device.push_back(e);if(n==2){check(selector==0);words.push_back(v);}return;}
 check(a>=ram && a<=ramEnd-n);rawWrite(a,n,v);
}
extern "C" {
unsigned m68k_read_memory_8(unsigned a){return read(a,1);}unsigned m68k_read_memory_16(unsigned a){return read(a,2);}unsigned m68k_read_memory_32(unsigned a){return read(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){write(a,1,v);}void m68k_write_memory_16(unsigned a,unsigned v){write(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){write(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return rawRead(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return rawRead(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return rawRead(a,4);}
void pokeri_exception(unsigned v){if(injecting && v==31){++irqEntries;return;}exceptionVector=v;check(v==46);}
}
static void interruptBoundary(){
 const auto before=mem;unsigned regs[16];
 for(unsigned i=0;i<16;++i)regs[i]=m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+i));
 const unsigned pc=m68k_get_reg(nullptr,M68K_REG_PC),sr=m68k_get_reg(nullptr,M68K_REG_SR);
 check(sr&0x2000);const unsigned count=irqEntries,events=device.size();
 injecting=true;m68k_set_irq(7);m68k_execute(1);m68k_set_irq(0);
 if(m68k_get_reg(nullptr,M68K_REG_PC)==irqStub)m68k_execute(1);
 injecting=false;check(irqEntries==count+1 && device.size()==events);
 check(m68k_get_reg(nullptr,M68K_REG_PC)==pc && m68k_get_reg(nullptr,M68K_REG_SR)==sr);
 for(unsigned i=0;i<16;++i)check(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+i))==regs[i]);
 const unsigned frame=regs[15]-6;
 check(rawRead(frame,2)==sr && rawRead(frame+2,4)==pc);
 for(unsigned a=ram;a<ramEnd;++a)if(a<frame || a>=frame+6)check(mem[a]==before[a]);
}
static std::vector<Boundary> run(unsigned consumer,unsigned producer,unsigned capacity,unsigned flags,bool user,bool error,unsigned ctl,int interruptAt=-1){
 running=false;std::fill(mem.begin()+ram,mem.begin()+ramEnd,0xa5);
 unsigned initial[16];for(unsigned i=0;i<16;++i)initial[i]=0x13579000+i*0x101;
 initial[14]=fields+30682;initial[15]=stack;
 const unsigned returnSr=(user?0:0x2000)|0x100|((flags*7)&31);
 rawWrite(fields,4,ring+producer*2);rawWrite(fields+4,4,ring+consumer*2);
 rawWrite(fields+152,4,ring);rawWrite(fields+156,4,ring+16);
 for(unsigned i=0;i<8;++i)rawWrite(ring+i*2,2,0x1100+i*0x123);
 rawWrite(irqStub,2,0x4e73); // Authored interrupt stub: RTE, no ROM-derived bytes.
 rawWrite(stack,2,returnSr);rawWrite(stack+2,4,returnedPc);
 ready=capacity;errorStatus=error?0x80:0;control=ctl;selector=0x91;exceptionVector=0;words.clear();device.clear();accesses.clear();
 m68k_set_irq(0);m68k_set_cpu_type(M68K_CPU_TYPE_68000);m68k_set_reg(M68K_REG_SR,0x2700);
 m68k_set_reg(M68K_REG_USP,userStack);
 for(unsigned i=0;i<16;++i)m68k_set_reg(m68k_register_t(M68K_REG_D0+i),initial[i]);
 m68k_set_reg(M68K_REG_SR,0x2200|flags);m68k_set_reg(M68K_REG_PC,first);
 running=true;std::vector<Boundary> states;unsigned cycles=0;
 for(;;){
  const unsigned pc=m68k_get_reg(nullptr,M68K_REG_PC);
  if(pc==returnedPc || pc==trapPc)break;
  check(pc>=first && pc<=errorEnd);check(states.size()<160);
  // A record describes the boundary after one original instruction, including
  // its bus operations and RAM stores. Keeping zero-access records is essential.
  if(int(states.size())==interruptAt)interruptBoundary();
  accesses.clear();cycles+=m68k_execute(1);Boundary b={};
  b.pc=m68k_get_reg(nullptr,M68K_REG_PC);b.sr=m68k_get_reg(nullptr,M68K_REG_SR);b.cycles=cycles;
  for(unsigned i=0;i<16;++i)b.regs[i]=m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+i));
  b.accesses=accesses;states.push_back(b);
  if(pc==normalEnd)check(!error && b.pc==returnedPc);
  if(pc==errorEnd)check(error && b.pc==trapPc);
 }
 running=false;check(!states.empty());const Boundary &last=states.back();
 for(unsigned i=0;i<15;++i)check(last.regs[i]==initial[i]);
 // The handler must leave its interrupted frame intact in either branch.
 check(rawRead(stack,2)==returnSr && rawRead(stack+2,4)==returnedPc);
 check(rawRead(fields,4)==ring+producer*2);
 if(error){
  check(exceptionVector==46 && last.pc==trapPc && last.regs[15]==stack-6);
  check(rawRead(stack-4,4)==errorEnd+2 && rawRead(fields+4,4)==ring+consumer*2);
  const unsigned trapSr=0x2200|(flags&16)|((ctl&0x3f)?0:4);
  check(rawRead(stack-6,2)==trapSr && last.sr==trapSr);
  check(words.empty() && device.size()==5 && selector==2);
  check(device[0].address==0xf6000 && !device[0].write && (device[0].value&128));
  check(device[1].write && device[1].address==0xf6000 && device[1].value==2);
  check(!device[2].write && device[2].address==0xf6002 && device[2].value==ctl);
  check(device[3].write && device[3].value==(ctl|0xc0));
  check(device[4].write && device[4].value==(ctl&0x3f));
 }else{
  check(!exceptionVector && last.pc==returnedPc && last.sr==returnSr);
  check(last.regs[15]==(user?userStack:stack+6));check(m68k_get_reg(nullptr,M68K_REG_ISP)==stack+6);
  // Ring contents define the expected FIFO stream independently of instruction
  // decoding. The one-past-end producer/consumer is a valid stored sentinel.
  unsigned cursor=consumer;std::vector<unsigned> expected;
  if(cursor!=producer){
   if(cursor==8)cursor=0;
   while(cursor!=producer && expected.size()<capacity){
    check(cursor<8);expected.push_back(0x1100+cursor*0x123);++cursor;
    if(cursor==8 && producer!=8)cursor=0;
    check(expected.size()<=8);
   }
  }
  check(words==expected && rawRead(fields+4,4)==ring+cursor*2 && selector==3);
  unsigned disable=0;for(const auto &e:device)if(e.write && e.address==0xf6002 && e.size==1){check(e.value==0x80);++disable;}
  check(disable==unsigned(consumer==producer));
 }
 // Outside the original saved-register/exception stack and consumer pointer,
 // this handler has no RAM stores. Guard the complete callback write set.
 for(const auto &b:states)for(const auto &e:b.accesses)if(e.write && e.address<ramEnd)
  check((e.address>=stack-16 && e.address+e.size<=stack) || (e.address==fields+4 && e.size==4));
 return states;
}
static void dump(std::ofstream &out,const char *name,const std::vector<Boundary>&states){
 out<<"{\"case\":\""<<name<<"\",\"boundaries\":[";bool comma=false;
 for(const auto &b:states){if(comma)out<<',';comma=true;out<<"{\"pc\":"<<b.pc<<",\"sr\":"<<b.sr<<",\"cycles\":"<<b.cycles<<",\"registers\":[";
  for(unsigned i=0;i<16;++i){if(i)out<<',';out<<b.regs[i];}out<<"],\"accesses\":[";bool next=false;
  for(const auto &e:b.accesses){if(next)out<<',';next=true;out<<'['<<e.address<<','<<e.size<<','<<e.write<<','<<e.value<<']';}out<<"]}";
 }out<<"]}\n";
}
int main(int argc,char**argv){
 check(argc==3);std::ifstream rom(argv[1],std::ios::binary);rom.read((char*)mem.data(),0x40000);check(rom.gcount()==0x40000);
 m68k_init();unsigned cases=0;unsigned long boundaries=0;
 for(unsigned c=0;c<=8;++c)for(unsigned p=0;p<=8;++p)for(unsigned n=0;n<=8;++n)
 for(unsigned f=0;f<32;++f)for(unsigned user=0;user<2;++user){boundaries+=run(c,p,n,f,user,false,0).size();++cases;}
 for(unsigned ctl=0;ctl<256;++ctl)for(unsigned f=0;f<32;++f)for(unsigned user=0;user<2;++user){boundaries+=run(6,2,8,f,user,true,ctl).size();++cases;}
 unsigned interruptedCases=0;
 struct Shape {unsigned c,p,n;bool error;};
 const Shape shapes[]={{3,3,8,false},{2,6,0,false},{2,6,2,false},{6,2,8,false},{8,2,8,false},{6,8,8,false},{6,2,8,true}};
 for(const auto &shape:shapes)for(unsigned flags=0;flags<32;++flags)for(unsigned user=0;user<2;++user){
  const auto baseline=run(shape.c,shape.p,shape.n,flags,user,shape.error,0xa5);
  for(unsigned at=0;at<baseline.size();++at){
   const auto resumed=run(shape.c,shape.p,shape.n,flags,user,shape.error,0xa5,int(at));
   check(resumed.size()==baseline.size());
   for(unsigned i=0;i<baseline.size();++i){const auto &a=baseline[i],&b=resumed[i];
    check(a.pc==b.pc && a.sr==b.sr && a.cycles==b.cycles && a.accesses.size()==b.accesses.size());
    for(unsigned r=0;r<16;++r)check(a.regs[r]==b.regs[r]);
    for(unsigned j=0;j<a.accesses.size();++j){const auto &x=a.accesses[j],&y=b.accesses[j];check(x.address==y.address && x.size==y.size && x.write==y.write && x.value==y.value);}
   }
   ++interruptedCases;
  }
 }
 check(irqEntries==interruptedCases);
 std::ofstream out(argv[2]);check(bool(out));
 dump(out,"empty",run(3,3,8,31,false,false,0));dump(out,"busy",run(2,6,0,0,true,false,0));
 dump(out,"partial",run(2,6,2,17,true,false,0));dump(out,"wrap",run(6,2,8,10,false,false,0));
 dump(out,"initial-wrap",run(8,2,8,4,false,false,0));dump(out,"end-sentinel",run(6,8,8,21,false,false,0));
 dump(out,"error",run(6,2,8,31,false,true,0xa5));check(bool(out));
 std::printf("PASS: %u real level-7 interrupt/RTE insertions; exact guest-boundary state, guest cycles and bus/store streams after resumption.\n",interruptedCases);
 std::printf("PASS: %u unmodified-ROM handler cases, %lu original instruction boundaries; all ring positions/readiness capacities/CCRs, both return stack modes, all error control bytes. Reference only; no native candidate comparison.\n",cases,boundaries);
}
