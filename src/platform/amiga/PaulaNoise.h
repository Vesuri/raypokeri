#ifndef POKERI_PAULA_NOISE_H
#define POKERI_PAULA_NOISE_H
#include <stdint.h>
extern "C" void pokeriNoiseFill(uint8_t *,uint32_t *,unsigned,unsigned);
namespace pokeri {
// Shared evolving noise, with two fixed square-wave gates. Pitch is provided
// by Paula, never by re-rendering a note. Mixed noise follows the tone's
// playback rate: this is the approved spectral approximation, not exact AY.
struct PaulaNoise {
    enum { Length=8192, Bytes=Length*3, Refresh=8 };
    uint32_t lfsr=0x1ace1; // avoid the reset seed's long initial all-low run
    unsigned cursor=0;
    void fill(uint8_t *data,unsigned begin,unsigned count){pokeriNoiseFill(data,&lfsr,begin,count);}
    void prepare(uint8_t *data){lfsr=0x1ace1;cursor=0;fill(data,0,Length);}
    void refresh(uint8_t *data){fill(data,cursor,Refresh);cursor+=Refresh;if(cursor==Length)cursor=0;}
};
// Bound a queued DMA slice to about 8 ms, except where one Paula word itself
// is longer. Powers of two divide the shared loop and preserve even DMA sizes.
inline unsigned paulaSlice(unsigned period){
    unsigned bytes=2;
    uint32_t clocks=uint32_t(period)<<1;
    while(bytes<256 && clocks<=14187){bytes<<=1;clocks<<=1;}
    return bytes;
}
// Use the longer fixed gate earlier than pure tones: this keeps DMA words
// short enough for prompt note changes throughout the game's mixed-tone range.
inline unsigned paulaMixedPeriod(unsigned tone,unsigned purePeriod){
    return tone>575 && tone<=2300?purePeriod>>2:purePeriod;
}
// One sample per noise shift, AY noise rate = 1 MHz / (16 * divider).
// Paula cannot exceed its safe DMA sample rate; high-rate noise is clamped.
inline void paulaNoisePeriods(uint16_t *periods){
    unsigned value=0,remainder=0;
    for(unsigned p=1;p<32;++p){
        value+=56;remainder+=750320;
        if(remainder>=1000000){remainder-=1000000;++value;}
        periods[p]=uint16_t(value<124?124:value);
    }
    periods[0]=periods[1];
}
}
#endif
