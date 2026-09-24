#ifndef POKERI_WINDOW_H
#define POKERI_WINDOW_H
#include "../src/board/Board.h"
#include "VideoOutput.h"
struct Window {
    void *window=nullptr,*renderer=nullptr,*texture=nullptr;
    unsigned width=0,height=0;uint64_t started=0,startCycle=0;
    bool enabled=false;
    ~Window();
    void open(uint64_t cycle);
    bool poll(pokeri::Board &board);
    void show(const pokeri::VideoFrame &frame,uint64_t cycle,unsigned cpuHz);
};
#endif
