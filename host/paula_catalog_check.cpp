// Research-only: execute the original sound-table reader for every record.
// The scheduler call returns immediately; its game code is never translated.
#include "musashi/m68k.h"
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
void m68k_write_memory_8(unsigned a,unsigned v){if(a<0x40000 || a>=0x80000)throw std::runtime_error("unexpected write");memory[a]=v;}
void m68k_write_memory_16(unsigned a,unsigned v){m68k_write_memory_8(a,v>>8);m68k_write_memory_8(a+1,v);}
void m68k_write_memory_32(unsigned a,unsigned v){m68k_write_memory_16(a,v>>16);m68k_write_memory_16(a+2,v);}
unsigned m68k_read_disassembler_8(unsigned a){return m68k_read_memory_8(a);}
unsigned m68k_read_disassembler_16(unsigned a){return m68k_read_memory_16(a);}
unsigned m68k_read_disassembler_32(unsigned a){return m68k_read_memory_32(a);}
}
int main()try{
    unsigned base=0;for(const char *name:{"77POK30","77POK38","77POK34","PARA200J"}){
        std::ifstream file(std::string("rom/")+name,std::ios::binary);
        if(!file.read((char*)memory.data()+base,65536))throw std::runtime_error("missing ROM");base+=65536;
    }
    std::ifstream file("tmp/paula-wave-records.csv");std::string line;std::getline(file,line);unsigned count=0;
    m68k_init();m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    while(std::getline(file,line)){
        unsigned offset=std::stoul(line,nullptr,16);std::string expected=line.substr(line.find(',')+1);
        std::fill(memory.begin()+0x40000,memory.begin()+0x80000,0);
        // Parameter record pointer, and original scheduler's RAM jump stub.
        m68k_write_memory_32(0x48b00-0x7b10,0x30000+offset);
        m68k_write_memory_16(0x48b00-0x6e48,0x4ef9);
        m68k_write_memory_32(0x48b00-0x6e48+2,0xd08); // existing ROM RTS
        m68k_set_reg(M68K_REG_SR,0x2700);m68k_set_reg(M68K_REG_SP,0x40b00);
        m68k_set_reg(M68K_REG_A6,0x48b00);m68k_set_reg(M68K_REG_PC,0xe058);
        m68k_write_memory_32(0x40b00,0x40000);
        unsigned n=0;while(m68k_get_reg(nullptr,M68K_REG_PC)!=0x40000 && n++<1000)m68k_execute(1);
        if(n>=1000)throw std::runtime_error("sound reader did not return");
        const unsigned masks[]={255,15,255,15,255,15,31,255,31,31,31,255,255,15};
        for(unsigned r=0;r<14;++r)if((memory[0x48b00-0x7908+r]&masks[r])!=std::stoul(expected.substr(r*2,2),nullptr,16))throw std::runtime_error("catalog differs from original ROM reader");
        ++count;
    }
    if(count!=125)throw std::runtime_error("incomplete catalog");
    printf("PASS: original ROM sound reader agrees with all %u catalog records\n",count);
}catch(const std::exception &e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
