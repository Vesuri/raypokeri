#include "musashi/m68k.h"
#include "../src/platform/amiga/PaulaNoise.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
static std::array<unsigned char,0x100000> memory{};
static unsigned acknowledgments,disables,irq;
static unsigned read(unsigned a,unsigned n){assert(a+n<=memory.size());unsigned v=0;while(n--)v=(v<<8)|memory[a++];return v;}
static void write(unsigned a,unsigned n,unsigned v){if(a==0xdff09a){assert(n==2 && v==irq);++disables;return;}if(a==0xdff09c){assert(n==2 && v==irq);++acknowledgments;return;}assert(a+n<=memory.size());while(n){--n;memory[a+n]=v;v>>=8;}}
extern "C" {
void pokeri_exception(unsigned){assert(false);}
unsigned m68k_read_memory_8(unsigned a){return read(a,1);}unsigned m68k_read_memory_16(unsigned a){return read(a,2);}unsigned m68k_read_memory_32(unsigned a){return read(a,4);}
void m68k_write_memory_8(unsigned a,unsigned v){write(a,1,v);}void m68k_write_memory_16(unsigned a,unsigned v){write(a,2,v);}void m68k_write_memory_32(unsigned a,unsigned v){write(a,4,v);}
unsigned m68k_read_disassembler_8(unsigned a){return read(a,1);}unsigned m68k_read_disassembler_16(unsigned a){return read(a,2);}unsigned m68k_read_disassembler_32(unsigned a){return read(a,4);}
}
int main(int argc,char **argv){
    assert(argc==3);std::ifstream f(argv[1],std::ios::binary);std::vector<unsigned char> code{std::istreambuf_iterator<char>(f),{}};assert(!code.empty());
    std::copy(code.begin(),code.end(),memory.begin()+0x1000);
    m68k_init();m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    unsigned checks=0,lo=1000000,hi=0;
    for(unsigned wait:{0u,1u,2u})for(unsigned streaming:{0u,1u})for(unsigned slice:{2u,4u,8u,16u,32u,64u,128u,256u})for(unsigned length=2;length<=1024;length+=2)for(unsigned offset:{0u,length-2,length})for(unsigned channel=0;channel<3;++channel){
        constexpr unsigned desc=0x90000,hw=0xa0000,begin=0x60000,stack=0x80000;
        irq=0x80<<channel;acknowledgments=disables=0;
        write(desc,4,begin);write(desc+4,4,begin+length);write(desc+8,4,begin+offset);
        write(desc+12,4,hw);write(desc+16,2,irq);write(desc+18,2,slice);write(desc+20,4,100);write(desc+24,2,170);write(desc+26,2,124);write(desc+28,2,wait);write(desc+30,2,streaming);write(hw+6,2,124);
        for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),0xabc000+r);
        m68k_set_reg(M68K_REG_A1,desc);m68k_set_reg(M68K_REG_SR,0x2400);
        m68k_set_reg(M68K_REG_SP,stack);write(stack,4,0x70000);m68k_set_reg(M68K_REG_PC,0x1000);
        unsigned cycles=0,steps=0;while(m68k_get_reg(nullptr,M68K_REG_PC)!=0x70000 && steps++<100)cycles+=m68k_execute(1);
        assert(steps<100);unsigned start=offset==length?0:offset,n=std::min(slice,length-start);
        assert(read(hw,4)==begin+start && read(hw+4,2)==n/2 && read(desc+8,4)==begin+start+n);
        assert(read(desc+28,2)==(wait?wait-1:0));
        assert(read(hw+6,2)==(wait==1?170u:124u));
        assert(read(desc+26,2)==(wait==1?170u:124u));
        assert(disables==unsigned(!streaming && wait<=1));
        assert(acknowledgments==2 && read(desc+20,4)==101);
        for(unsigned r=2;r<15;++r)if(r!=8 && r!=9)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==0xabc000+r);
        assert(m68k_get_reg(nullptr,M68K_REG_A1)==desc && m68k_get_reg(nullptr,M68K_REG_SP)==stack+4 && !m68k_get_reg(nullptr,M68K_REG_D0));
        lo=std::min(lo,cycles);hi=std::max(hi,cycles);++checks;
    }
    printf("PASS: %u linked 68000 DMA-server cases, registers, pointer/length/wrap and double ACK; %u..%u core cycles (excludes Exec/exception/trace/DMA contention)\n",checks,lo,hi);
    std::ifstream nf(argv[2],std::ios::binary);
    std::vector<unsigned char> noiseCode{std::istreambuf_iterator<char>(nf),{}};
    assert(noiseCode.size()>4);
    unsigned noisePc=(noiseCode[0]<<24)|(noiseCode[1]<<16)|(noiseCode[2]<<8)|noiseCode[3];
    std::copy(noiseCode.begin()+4,noiseCode.end(),memory.begin());
    unsigned refillLo=~0u,refillHi=0,initCycles=0;
    for(unsigned seed:{1u,0x1ace1u,0x1ffffu})for(unsigned count:{0u,8u,8192u})for(unsigned start:{0u,4096u,8184u}){
        if(start+count>8192)continue;
        constexpr unsigned data=0x50000,state=0x90000,stack=0x80000;
        alignas(4) std::array<uint8_t,pokeri::PaulaNoise::Bytes> expected;expected.fill(42);
        uint32_t expectedState=seed;pokeriNoiseFill(expected.data(),&expectedState,start,count);
        std::fill(memory.begin()+data-1,memory.begin()+data+expected.size()+1,42);
        write(state,4,seed);write(stack,4,0x70000);write(stack+4,4,data);
        write(stack+8,4,state);write(stack+12,4,start);write(stack+16,4,count);
        for(unsigned r=0;r<15;++r)m68k_set_reg(m68k_register_t(M68K_REG_D0+r),0xabc000+r);
        m68k_set_reg(M68K_REG_SR,0x2000);m68k_set_reg(M68K_REG_SP,stack);m68k_set_reg(M68K_REG_PC,noisePc);
        unsigned cycles=0,steps=0;
        while(m68k_get_reg(nullptr,M68K_REG_PC)!=0x70000 && steps++<500000)cycles+=m68k_execute(1);
        assert(steps<500000 && read(state,4)==expectedState);
        assert(std::equal(expected.begin(),expected.end(),memory.begin()+data));
        assert(memory[data-1]==42 && memory[data+expected.size()]==42);
        for(unsigned r=2;r<15;++r)if(r!=8 && r!=9)assert(m68k_get_reg(nullptr,m68k_register_t(M68K_REG_D0+r))==0xabc000+r);
        if(count==8){refillLo=std::min(refillLo,cycles);refillHi=std::max(refillHi,cycles);}
        if(count==8192)initCycles=std::max(initCycles,cycles);
    }
    printf("PASS: linked noise generator, buffers and preserved registers; refresh %u..%u core cycles; initialization <=%u cycles (excludes Chip DMA contention)\n",refillLo,refillHi,initCycles);

}
