#ifndef POKERI_RELOCATION_H
#define POKERI_RELOCATION_H
#include <array>
#include <cstdint>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

// CPU-visible placements; Board storage remains indexed by board-local address.
// There is deliberately no alias at an original address in relocated mode.
struct Relocation {
    uint32_t rom=0,ram=0x40000,guard=0x80000;
    bool enabled=false,resetVectors=false,bypass=false;
    std::array<uint8_t,32> vectorShadow{};
    struct LowHook {unsigned reg,offset,size;};
    std::map<unsigned,LowHook> lowHooks;
    void loadLowHooks(const std::string &path){
        std::ifstream f(path);std::string line;
        if(!f || !std::getline(f,line) || line!="pc,address_register,offset,size")throw std::runtime_error("invalid low-vector hook table");
        while(std::getline(f,line)){
            for(char &c:line)if(c==',')c=' ';
            std::istringstream row(line);unsigned pc,reg,offset,size;std::string extra;
            if(!(row>>std::hex>>pc>>std::dec>>reg>>offset>>size) || (row>>extra) || pc>=0x40000 || (pc&1) || reg>7 || offset>28 || size!=4 ||
               !lowHooks.emplace(pc,LowHook{reg,offset,size}).second)throw std::runtime_error("invalid low-vector hook descriptor");
        }
    }
    struct ControlHook {std::string kind;unsigned target;};
    std::map<unsigned,ControlHook> controls;
    void loadControls(const std::string &path){
        std::ifstream f(path);std::string line;
        if(!f || !std::getline(f,line) || line!="pc,operation,target")throw std::runtime_error("invalid control hook table");
        while(std::getline(f,line)){
            for(char &c:line)if(c==',')c=' ';
            std::istringstream row(line);unsigned pc,target;std::string kind,extra;
            if(!(row>>std::hex>>pc>>kind>>target) || (row>>extra) || pc>0x3fffc || (pc&1) || target>=0x40000 || (target&1) ||
               (kind!="skip_module_checksum" && kind!="set_ram_delta_d7") || !controls.emplace(pc,ControlHook{kind,target}).second)
                throw std::runtime_error("invalid control hook descriptor");
        }
    }
    std::set<unsigned> resetHooks;
    void loadResetHooks(const std::string &path){
        std::ifstream f(path);std::string line;
        if(!f || !std::getline(f,line) || line!="pc,operation")throw std::runtime_error("invalid RESET hook table");
        while(std::getline(f,line)){
            for(char &c:line)if(c==',')c=' ';
            std::istringstream row(line);unsigned pc;std::string kind,extra;
            if(!(row>>std::hex>>pc>>kind) || (row>>extra) || pc>=0x40000 || (pc&1) || kind!="peripheral_reset" || !resetHooks.insert(pc).second)
                throw std::runtime_error("invalid RESET hook descriptor");
        }
    }
    uint32_t canonical(uint32_t a) const {
        if(!enabled)return a&0xfffff;
        if(a>=rom && a-rom<0x40000)return a-rom;
        if(a>=ram && a-ram<0x40000)return a-ram+0x40000;
        if(a>=guard && a-guard<0x80000)return a-guard+0x80000;
        return 0xffffffff;
    }
    void validate() const {
        if(!enabled)return;
        if(rom&255)throw std::runtime_error("ROM base must preserve 256-byte module alignment");
        uint32_t bases[]={rom,ram,guard},sizes[]={0x40020,0x40000,0x80000};
        for(unsigned i=0;i<3;++i){
            if((bases[i]&1) || bases[i]<0x100000 || bases[i]>0x1000000-sizes[i])
                throw std::runtime_error("relocated ranges must be even and above old board space within 24 bits");
            for(unsigned j=0;j<i;++j)if(bases[i]<bases[j]+sizes[j] && bases[j]<bases[i]+sizes[i])
                throw std::runtime_error("overlapping relocated ranges");
        }
        if(!bypass)throw std::runtime_error("relocation requires explicit --bypass-module-checksums");
    }
    template<size_t N> void patch(std::array<uint8_t,N> &m,const std::string &path) {
        auto get=[&](unsigned a){return (uint32_t(m[a])<<24)|(uint32_t(m[a+1])<<16)|(uint32_t(m[a+2])<<8)|m[a+3];};
        auto put=[&](unsigned a,uint32_t v){for(unsigned i=0;i<4;++i)m[a+i]=v>>(24-8*i);};
        for(unsigned i=0;i<vectorShadow.size();++i)vectorShadow[i]=m[i];
        // The user explicitly authorized bypassing runtime module validation.
        // Keep header/entry validation and the original data/relocation loader.
        if(bypass)for(const auto &entry:controls)if(entry.second.kind=="skip_module_checksum"){
            unsigned pc=entry.first;int displacement=int(entry.second.target)-int(pc+2);
            if(displacement<-32768 || displacement>32767)throw std::runtime_error("control branch out of range");
            m[pc]=0x60;m[pc+1]=0;m[pc+2]=uint16_t(displacement)>>8;m[pc+3]=displacement;
        }
        if(!enabled)return;
        std::ifstream f(path);std::string line;
        if(!f || !std::getline(f,line) || line!="offset,kind")throw std::runtime_error("invalid relocation table");
        std::set<unsigned> touched;
        while(std::getline(f,line)){
            auto comma=line.find(',');if(comma==std::string::npos)throw std::runtime_error("invalid relocation row");
            size_t end;unsigned long wide=std::stoul(line.substr(0,comma),&end,16);
            if(wide>0x3fffc)throw std::runtime_error("relocation offset exceeds ROM");unsigned a=wide;
            if(end!=comma || a>0x3fffc || (a&1) || !touched.insert(a).second || touched.count(a+2) || (a>=2 && touched.count(a-2)))throw std::runtime_error("invalid relocation offset");
            std::string kind=line.substr(comma+1);uint32_t v=get(a);
            if(kind=="rom"){if(v>=0x40000)throw std::runtime_error("ROM relocation operand mismatch");v+=rom;}
            else if(kind=="ram"){if(v<0x40000 || v>=0x80000)throw std::runtime_error("RAM relocation operand mismatch");v+=ram-0x40000;}
            else if(kind=="ram_addend"){if(v!=0x20000)throw std::runtime_error("RAM addend mismatch");v+=ram-0x40000;}
            else if(kind=="device"){if(v<0x80000 || v>=0x100000)throw std::runtime_error("device relocation operand mismatch");v+=guard-0x80000;}
            else throw std::runtime_error("unknown relocation kind");
            put(a,v);
        }
    }
};
#endif
