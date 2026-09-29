#ifndef POKERI_NATIVE_IRQ_CACHE_H
#define POKERI_NATIVE_IRQ_CACHE_H
#include <stdint.h>
#include "board/Board.h"
namespace pokeri {
// Native service cache, not hardware state. Video status always stays live:
// inline FIFO/raster paths can change it without calling the shared bus.
struct IrqCache {
    uint16_t state=0; // unknown / valid clear / valid pending = 0 / 1 / 3
    void invalidate(){state=0;}
    void beforeAccess(uint32_t address,unsigned size){
        if((address<=0xfb003 && address+size>0xfb002) ||
           (address<=0xfb017 && address+size>0xfb014))invalidate();
    }
    // Byte endpoints know their direction. PIA output/DDR writes and
    // control reads do not change IRQ state; neither do ACIA status reads
    // or TX writes. Keep conservative span invalidation for generic buses.
    template<bool Writing> void beforeByte(uint32_t address){
        if((address&~2u)==(Writing?0xfb015u:0xfb014u) ||
           address==(Writing?0xfb002u:0xfb003u))invalidate();
    }
    bool peripherals(const Board &board){
        if(!state)state=board.pia[0].Pia6821::irq() || board.serial[0].Acia6850::irq()?3:1;
        return (state&2)!=0;
    }
    unsigned level(const Board &board,uint8_t status){
        return peripherals(board) || (status&board.video.control[3])?5:0;
    }
};
}
#endif
