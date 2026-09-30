#include "acrtc_duration.h"
#include <cstdio>
#include <stdexcept>
using pokeri_research::AcrtcDuration;
static void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(){try{
    pokeri::Hd63484 v;uint64_t n;bool inferred;
    auto expect=[&](std::vector<uint16_t>w,uint64_t expected,bool guess=false){
        check(AcrtcDuration::counts(v,w,n,inferred) && n==expected && inferred==guess,"manual count/geometry fixture");
    };
    expect({0x0400,0,0},8);expect({0x0800,1},6);expect({0x0c00},6);
    expect({0x1800,3,0,0,0},20);expect({0x4400},12);expect({0x4800,0},8);
    expect({0x5800,0,3,0xfffe},60);expect({0x8000,0,0},56);
    expect({0x8c00,5,3},38);expect({0x9c00,2,5,3,0xfffb,0xfffd},80);
    expect({0xc400,3,0xfffe},90);expect({0xcc00},18);
    expect({0xd000,0x0203},98);expect({0xe000,0,0,3,0xfffe},172);
    check(!AcrtcDuration::counts(v,{0x0800},n,inferred),"truncated command rejected");
    check(!AcrtcDuration::counts(v,{0x0820,0},n,inferred),"reserved bits rejected");
    check(!AcrtcDuration::counts(v,{0x8401,0,0},n,inferred),"invalid move rejected");
    check(!AcrtcDuration::counts(v,{0x1c00,1},n,inferred),"unimplemented command refused");
    v.control[2]=2;v.control[0xc3]=4;v.origin=0x10000;
    v.parameter[0]=v.parameter[1]=0x1111;v.parameter[3]=0xffff;
    auto before=v.frame;auto registers=v.parameter;
    expect({0xa900,2},162,true);
    check(v.frame==before && v.parameter==registers && !v.curveCacheMisses && !v.commands[42],"geometry trial does not mutate renderer");
    std::fill(v.frame.begin(),v.frame.end(),0xffff);
    for(unsigned y=1;y<=2;++y)for(unsigned x=1;x<=3;++x)v.frame[0x1000-y*4+x/4]&=uint16_t(~(15u<<((x&3)*4)));
    v.parameter[0x12]=v.parameter[0x13]=1;before=v.frame;registers=v.parameter;
    expect({0xc800},254,true); // (18*3+102)*2-58, a three-by-two region.
    check(v.frame==before && v.parameter==registers && !v.commands[50],"PAINT dry run does not commit pixels or CP");
    v.frame[0x1000-2*4]&=uint16_t(~(15u<<0)); // one extra pixel on second row
    expect({0xc800},272,true); // seven dots, two runs, explicitly inferred.
    check(AcrtcDuration::convert(6,8000000,3000000,n) && n==16,"exact rate conversion");
    check(AcrtcDuration::convert(8,8000000,3000000,n) && n==22,"fractional duration rounds up");
    check(!AcrtcDuration::convert(UINT64_MAX,8000000,3000000,n),"conversion overflow rejected");
    check(!AcrtcDuration::convert(8,8000000,0,n),"zero rate rejected");
    puts("PASS: table formulas, signed geometry, private curve/PAINT observation, malformed commands and bounded rate conversion");return 0;
}catch(const std::exception&e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
