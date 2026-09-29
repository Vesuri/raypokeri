#include "board/WordMath.h"
extern "C" uint64_t pokeriTickProduct(uint32_t cycles,uint32_t rate){
    return pokeri::wideProduct32(cycles,rate);
}
