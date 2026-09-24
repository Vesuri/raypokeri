#ifndef POKERI_ACCESS_GATE_H
#define POKERI_ACCESS_GATE_H
#include <cstdint>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>

// Host audit gate, not an implementation of the future native Line-A handler.
// Each descriptor permits exactly one observed PC/address/width/direction.
class AccessGate {
    using Key=std::tuple<uint32_t,uint32_t,unsigned,char>;
    std::set<Key> entries;
    bool enabled=false;
public:
    bool active() const {return enabled;}
    void load(const std::string &path) {
        std::ifstream file(path);std::string line;
        if(!file || !std::getline(file,line) || line!="pc,address,size,direction")
            throw std::runtime_error("invalid I/O access table header");
        std::set<Key> pending;
        while(std::getline(file,line)) {
            for(char &c:line)if(c==',')c=' ';
            std::istringstream row(line);uint32_t pc,address;unsigned size;char direction;std::string extra;
            if(!(row>>std::hex>>pc>>address>>std::dec>>size>>direction) || (row>>extra) ||
               pc>=0x40000 || (pc&1) || address<0x80000 || address>=0x100000 ||
               (size!=1 && size!=2 && size!=4) || address+size>0x100000 ||
               (direction!='R' && direction!='W') || !pending.insert(Key(pc,address,size,direction)).second)
                throw std::runtime_error("invalid/duplicate I/O access descriptor");
        }
        if(file.bad() || pending.empty())throw std::runtime_error("empty/unreadable I/O access table");
        entries.swap(pending);enabled=true;
    }
    bool permits(uint32_t pc,uint32_t address,unsigned size,char direction) const {
        return !enabled || entries.count(Key(pc,address,size,direction));
    }
};
#endif
