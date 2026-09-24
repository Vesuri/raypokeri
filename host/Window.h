#ifndef POKERI_WINDOW_H
#define POKERI_WINDOW_H
#include "../src/board/Board.h"
#include "VideoOutput.h"
struct Window : pokeri::Tone {
    void *window=nullptr,*renderer=nullptr,*texture=nullptr;
    unsigned width=0,height=0;uint64_t started=0,startCycle=0;
    bool enabled=false;
    unsigned audioDevice=0,audioCount=0;
    int16_t audioBuffer[882]; // 20 ms at the AY renderer's 44.1 kHz.
    ~Window();
    void open(uint64_t cycle);
    void openAudio();
    void sample(int16_t value) override;
    void flushAudio();
    void finishAudio();
    bool poll(pokeri::Board &board);
    void show(const pokeri::VideoFrame &frame,uint64_t cycle,unsigned cpuHz);
};
#endif
