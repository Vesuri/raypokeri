#ifndef POKERI_RETAINED_ACCOUNTING_H
#define POKERI_RETAINED_ACCOUNTING_H
#include <array>
#include <cstdint>
namespace pokeri {
// Active ROM accounting/context and its three mirrors. This block is identical
// at both validated relocation placements; runtime pointer metadata lies below
// it and is deliberately not retained. Original boot validates/repairs records.
// The format is tied to this ROM set and these bounds, not a CPU snapshot.
struct RetainedAccounting {
    static constexpr unsigned begin=0x43e60,end=0x44200,size=end-begin;
    std::array<uint8_t,8+size+4> bytes{};
    // Authored installer slot, not retained game state. The entire fixed-size
    // image must match; accepting it must leave guest RAM and cold-boot policy
    // unchanged. A normal save replaces it with the existing PKAC0001 format.
    void fresh(){
        bytes.fill(0);
        const uint8_t magic[8]={'P','K','A','F','0','0','0','1'};
        for(unsigned i=0;i<8;++i)bytes[i]=magic[i];
    }
    bool isFresh()const{
        const uint8_t magic[8]={'P','K','A','F','0','0','0','1'};
        for(unsigned i=0;i<8;++i)if(bytes[i]!=magic[i])return false;
        for(unsigned i=8;i<bytes.size();++i)if(bytes[i])return false;
        return true;
    }
    static uint32_t checksum(const uint8_t *p,unsigned count){
        uint32_t crc=0xffffffffu;
        while(count--){crc^=*p++;for(unsigned i=0;i<8;++i)crc=(crc>>1)^((crc&1)?0xedb88320u:0);}
        return ~crc;
    }
    void encode(const uint8_t *memory){
        const uint8_t magic[8]={'P','K','A','C','0','0','0','1'};
        for(unsigned i=0;i<8;++i)bytes[i]=magic[i];
        for(unsigned i=0;i<size;++i)bytes[8+i]=memory[begin+i];
        uint32_t crc=checksum(bytes.data(),8+size);
        for(unsigned i=0;i<4;++i)bytes[8+size+i]=uint8_t(crc>>(24-8*i));
    }
    bool valid()const{
        const uint8_t magic[8]={'P','K','A','C','0','0','0','1'};
        for(unsigned i=0;i<8;++i)if(bytes[i]!=magic[i])return false;
        uint32_t crc=0;for(unsigned i=0;i<4;++i)crc=(crc<<8)|bytes[8+size+i];
        return crc==checksum(bytes.data(),8+size);
    }
    bool decode(uint8_t *memory)const{
        if(!valid())return false;
        for(unsigned i=0;i<size;++i)memory[begin+i]=bytes[8+i];
        return true;
    }
};
}
#endif
