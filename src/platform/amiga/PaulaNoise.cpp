#include "PaulaNoise.h"
namespace {
// Four successive AY noise outputs are the low four state bits. Advance the
// shift register four steps at once; the 16 feedback/output words are authored
// logic tables, independent of game sounds. No per-note data or floating point.
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
constexpr uint32_t pack(unsigned a,unsigned b,unsigned c,unsigned d){return (a<<24)|(b<<16)|(c<<8)|d;}
#else
constexpr uint32_t pack(unsigned a,unsigned b,unsigned c,unsigned d){return a|(b<<8)|(c<<16)|(d<<24);}
#endif
constexpr uint32_t samples(unsigned n){return pack(n&1?127:129,n&2?127:129,n&4?127:129,n&8?127:129);}
static const uint32_t outputs[16]={samples(0),samples(1),samples(2),samples(3),samples(4),samples(5),samples(6),samples(7),samples(8),samples(9),samples(10),samples(11),samples(12),samples(13),samples(14),samples(15)};
static const uint32_t feedback[16]={0<<13,1<<13,2<<13,3<<13,4<<13,5<<13,6<<13,7<<13,8<<13,9<<13,10<<13,11<<13,12<<13,13<<13,14<<13,15<<13};
}
// Storage and ranges are longword-aligned; prepare and refresh satisfy this.
extern "C" void pokeriNoiseFill(uint8_t *data,uint32_t *state,unsigned begin,unsigned count){
    uint32_t noise=*state;
    auto *raw=reinterpret_cast<uint32_t*>(data);
    auto *mixed=raw+pokeri::PaulaNoise::Length/4;
    auto *low=mixed+pokeri::PaulaNoise::Length/4;
    auto *medium=low+pokeri::PaulaNoise::Length/4;
    auto *longGate=medium+pokeri::PaulaNoise::Length/4;
    for(unsigned i=begin/4,end=(begin+count)/4;i<end;++i){
        const uint32_t sample=outputs[noise&15];
        noise=(noise>>4)|feedback[(noise^(noise>>3))&15];
        raw[i]=sample;
        mixed[i]=(sample&pack(255,0,255,0))|pack(0,129,0,129);
        low[i]=(i&1)?pack(129,129,129,129):sample;
        medium[i]=(i&4)?pack(129,129,129,129):sample;
        longGate[i]=(i&16)?pack(129,129,129,129):sample;
    }
    *state=noise;
}
