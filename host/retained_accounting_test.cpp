#include "../src/RetainedAccounting.h"
#include <cassert>
#include <cstdio>
static std::array<uint8_t,0x80000> memory,restored;
int main(){
    using pokeri::RetainedAccounting;
    for(unsigned i=0;i<memory.size();++i)memory[i]=uint8_t((i>>4)^i);
    RetainedAccounting image;image.encode(memory.data());
    restored.fill(0x55);assert(image.decode(restored.data()));
    for(unsigned i=0;i<memory.size();++i)assert(restored[i]==(i>=image.begin && i<image.end?memory[i]:0x55));
    const auto good=image.bytes;
    restored.fill(0x55);
    for(unsigned i=0;i<image.bytes.size();++i)for(unsigned bit=0;bit<8;++bit){
        image.bytes=good;image.bytes[i]^=1u<<bit;assert(!image.decode(restored.data()));
    }
    for(auto value:restored)assert(value==0x55); // reject before touching RAM
    assert(!image.isFresh());
    image.fresh();assert(image.isFresh());assert(!image.decode(restored.data()));
    const auto fresh=image.bytes;
    for(unsigned i=0;i<fresh.size();++i)for(unsigned bit=0;bit<8;++bit){
        image.bytes=fresh;image.bytes[i]^=1u<<bit;
        assert(!image.isFresh());assert(!image.decode(restored.data()));
    }
    for(auto value:restored)assert(value==0x55);
    image.bytes.fill(0);assert(!image.isFresh());assert(!image.decode(restored.data()));
    image.bytes=good;assert(!image.isFresh());assert(image.decode(restored.data()));
    puts("PASS: retained accounting and authored fresh slot, exact bounds and every single-bit corruption");
}
