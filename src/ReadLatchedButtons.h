#ifndef POKERI_READ_LATCHED_BUTTONS_H
#define POKERI_READ_LATCHED_BUTTONS_H
#include <stdint.h>
namespace pokeri {
// External button transitions, separate from emulated device state. A level
// persists until a real input-data read observes it; timers do not acknowledge.
struct ReadLatchedButtons {
    uint16_t pending[8]={};
    uint8_t down=0,observed=255;
    bool append(unsigned bit,unsigned count){
        if(count>65535u-pending[bit])return false;
        pending[bit]=uint16_t(pending[bit]+count);return true;
    }
    uint8_t advance(){
        for(unsigned bit=0;bit<8;++bit){unsigned mask=1u<<bit;
            if(pending[bit] && (observed&mask)){
                --pending[bit];down^=mask;observed&=uint8_t(~mask);
            }
        }
        return down;
    }
    void read(uint8_t value,uint8_t inputMask){
        observed|=uint8_t((value^down)&inputMask); // active-low pins agree
    }
};
}
#endif
