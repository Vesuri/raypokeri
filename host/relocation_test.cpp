// Synthetic operands and control branches only; no program or ROM bytes.
#include "Relocation.h"
#include <cstdio>
#include <functional>
#include <memory>
static void check(bool b,const char *why){if(!b)throw std::runtime_error(why);}
static void rejects(const std::function<void()> &f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,"invalid relocation accepted");}
static void file(const char *path,const char *text){FILE*f=std::fopen(path,"w");if(!f)throw std::runtime_error("test file");std::fputs(text,f);std::fclose(f);}
int main()try{
    Relocation r;r.enabled=r.bypass=true;r.rom=0x100000;r.ram=0x200000;r.guard=0x300000;r.validate();
    check(r.canonical(0x100024)==0x24 && r.canonical(0x200456)==0x40456 && r.canonical(0x37b014)==0xfb014,"relocated address decoding");
    for(unsigned a:{0u,0x40000u,0xfb014u,0x140000u,0x280000u})check(r.canonical(a)==0xffffffff,"original/unmapped address aliased");
    auto invalid=r;invalid.ram=r.rom+0x20000;rejects([&]{invalid.validate();});
    invalid=r;invalid.rom+=2;rejects([&]{invalid.validate();});
    invalid=r;invalid.guard=0xff0000;rejects([&]{invalid.validate();});
    invalid=r;invalid.bypass=false;rejects([&]{invalid.validate();});
    auto image=std::unique_ptr<std::array<uint8_t,0x80000>>(new std::array<uint8_t,0x80000>{});
    auto put=[&](unsigned a,unsigned v){for(unsigned i=0;i<4;++i)(*image)[a+i]=v>>(24-8*i);};
    auto get=[&](unsigned a){unsigned v=0;for(unsigned i=0;i<4;++i)v=(v<<8)|(*image)[a+i];return v;};
    put(0,0x40100);put(4,0x100);put(8,0xfb000);put(12,0x20000);
    file("tmp/synthetic-relocations.csv","offset,kind\n000000,ram\n000004,rom\n000008,device\n00000c,ram_addend\n");
    file("tmp/synthetic-control.csv","pc,operation,target\n000200,skip_module_checksum,000222\n000300,set_ram_delta_d7,000000\n");
    r.loadControls("tmp/synthetic-control.csv");r.patch(*image,"tmp/synthetic-relocations.csv");
    check(get(0)==0x200100 && get(4)==0x100100 && get(8)==0x37b000 && get(12)==0x1e0000,"relocated operand values");
    check(get(0x200)==0x60000020,"checksum-skip branch assembled from offsets");
    check(r.vectorShadow[1]==4 && r.vectorShadow[5]==0,"original vector-data shadow preserved");
    file("tmp/synthetic-overlap.csv","offset,kind\n000020,rom\n000022,rom\n");
    rejects([&]{r.patch(*image,"tmp/synthetic-overlap.csv");});
    file("tmp/synthetic-low.csv","pc,address_register,offset,size\n000100,8,4,4\n");
    rejects([&]{r.loadLowHooks("tmp/synthetic-low.csv");});
    puts("PASS: relocated ranges, no old aliases, placement rejection, operand fixups, offset-based control branches and vector shadow");
}catch(const std::exception &e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
