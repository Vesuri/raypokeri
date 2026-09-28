// Research-only: call the original face-up producer for all suit/rank selectors.
// Initialize its ABI, two original RAM jump stubs and an isolated output ring.
// The complete routine and its list feeders execute unmodified in Musashi.
#include "musashi/m68k.h"
#include "../amiga/generated/CardBackRecipe.h"
#include <vector>
#include <array>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
static std::array<unsigned char,0x100000> memory{};
extern "C" {
void pokeri_exception(unsigned){throw std::runtime_error("unexpected CPU exception");}
unsigned m68k_read_memory_8(unsigned a){return memory.at(a);}
unsigned m68k_read_memory_16(unsigned a){return (m68k_read_memory_8(a)<<8)|m68k_read_memory_8(a+1);}
unsigned m68k_read_memory_32(unsigned a){return (m68k_read_memory_16(a)<<16)|m68k_read_memory_16(a+2);}
void m68k_write_memory_8(unsigned a,unsigned v){
    // Original ring-kick helper only selects CCR-low, enables IRQs, and
    // reselects the FIFO. The isolated producer does not consume the ring.
    static unsigned selected=0;
    if(a==0xf6000){if(v!=0 && v!=3)throw std::runtime_error("unexpected video selection");selected=v;return;}
    if(a==0xf6002){if(selected!=3 || v!=0x81)throw std::runtime_error("unexpected video write");return;}
    if(a<0x40000 || a>=0x80000)throw std::runtime_error("unexpected write");
    memory[a]=v;
}
void m68k_write_memory_16(unsigned a,unsigned v){m68k_write_memory_8(a,v>>8);m68k_write_memory_8(a+1,v);}
void m68k_write_memory_32(unsigned a,unsigned v){m68k_write_memory_16(a,v>>16);m68k_write_memory_16(a+2,v);}
unsigned m68k_read_disassembler_8(unsigned a){return m68k_read_memory_8(a);}
unsigned m68k_read_disassembler_16(unsigned a){return m68k_read_memory_16(a);}
unsigned m68k_read_disassembler_32(unsigned a){return m68k_read_memory_32(a);}
}
int main(int argc,char **argv)try{
    unsigned base=0;for(const char *name:{"77POK30","77POK38","77POK34","PARA200J"}){
        std::ifstream file(std::string("rom/")+name,std::ios::binary);
        if(!file.read((char*)memory.data()+base,65536))throw std::runtime_error("missing ROM");base+=65536;
    }
    // A6 is the original initialized runtime base. Only these two stubs and
    // the white colour parameter are read by the producer and its callees.
    for(auto stub:{std::array<unsigned,2>{0x42012,0x2ec6},std::array<unsigned,2>{0x41e26,0x4256}}){
        m68k_write_memory_16(stub[0],0x4ef9);m68k_write_memory_32(stub[0]+2,stub[1]);
    }
    m68k_write_memory_16(0x48b00-0x7722,0xffff);
    std::ofstream output(argc>1?argv[1]:"tmp/faceup-catalog.words");
    if(!output)throw std::runtime_error("cannot create local catalog");
    unsigned cards=0,striped=0,pictures=0,jokers=0,blanks=0,copies=0,rotated=0;
    const auto baseline=memory;
    m68k_init();m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    for(unsigned suit=1;suit<=4;++suit)for(unsigned rank=0;rank<=14;++rank){
        memory=baseline;
        m68k_write_memory_32(0x48b00-0x77da,0x50000);
        m68k_write_memory_32(0x48b00-0x77d6,0x50000);
        m68k_write_memory_32(0x48b00-0x7742,0x50000);
        m68k_write_memory_32(0x48b00-0x773e,0x70000);
        m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,0x40b00);
        m68k_set_reg(M68K_REG_A6,0x48b00);m68k_set_reg(M68K_REG_PC,0x1f69c);
        m68k_set_reg(M68K_REG_D0,suit);m68k_set_reg(M68K_REG_D1,rank);
        m68k_write_memory_32(0x40b00,0x40000);
        m68k_write_memory_32(0x40b04,0);m68k_write_memory_32(0x40b08,0);
        unsigned n=0;while(m68k_get_reg(nullptr,M68K_REG_PC)!=0x40000 && n++<100000)m68k_execute(1);
        if(n>=100000)throw std::runtime_error("card renderer did not return");
        unsigned end=m68k_read_memory_32(0x48b00-0x77da);
        if(end<0x50000 || end>=0x70000)throw std::runtime_error("ring overrun");
        std::vector<unsigned> words;
        for(unsigned a=0x50000;a<end;a+=2)words.push_back(m68k_read_memory_16(a));
        const auto *recipe=pokeri::card_recipe::words;
        unsigned prefix=pokeri::card_recipe::offsets[29];
        if(words.size()<prefix)throw std::runtime_error("incomplete common prefix");
        for(unsigned i=0;i<prefix;++i)if(words[i]!=recipe[i])throw std::runtime_error("white-card prefix differs");
        // Every command after the rounded white prefix is an AMOVE/AGCPY
        // pair. The original producer already reuses resident artwork; a
        // second immutable cache would duplicate it and need invalidation.
        for(unsigned at=prefix;at<words.size();at+=8){
            if(at+8>words.size() || words[at]!=0x8000 ||
               (words[at+3]!=0xe000 && words[at+3]!=0xe300))
                throw std::runtime_error("face artwork is not a resident copy");
            ++copies;rotated+=words[at+3]==0xe300;
        }
        if(rank==0){
            if(words.size()!=prefix+8 || words[prefix+3]!=0xe000 ||
               words[prefix+4]!=400 || words[prefix+5]!=uint16_t(-995) ||
               words[prefix+6]!=79 || words[prefix+7]!=88)
                throw std::runtime_error("joker resident image differs");
            ++jokers;
        }else if(rank==1){
            if(words.size()!=prefix)throw std::runtime_error("blank selector draws additional artwork");
            ++blanks;
        }
        if(rank>=2){
            // Four 17x17 suit/rank copies precede one 40x54 inset. Each
            // AMOVE+AGCPY pair contains eight words; no opcode wildcards.
            unsigned inset=prefix+4*8;
            if(words.size()<inset+8 || words[inset]!=0x8000 || words[inset+3]!=0xe000 ||
               words[inset+5]!=uint16_t(-1000) || words[inset+6]!=39 || words[inset+7]!=53)
                throw std::runtime_error("face-up inset shape/order differs");
            unsigned x=words[inset+4];
            if(rank>=11 && rank<=13){if(x!=200+(rank-11)*50)throw std::runtime_error("picture inset differs");++pictures;}
            else {if(x!=0)throw std::runtime_error("striped inset differs");++striped;}
        }
        output<<"CARD "<<std::dec<<suit<<" "<<rank;
        for(unsigned w:words)output<<" "<<std::hex<<w;
        output<<"\n";++cards;

    }
    printf("PASS: %u original face-up producers share white prefix; %u striped / %u picture insets\n",cards,striped,pictures);
    printf("PASS: %u joker / %u blank selectors; all %u artwork copies resident, %u rotated\n",jokers,blanks,copies,rotated);
}catch(const std::exception &e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
