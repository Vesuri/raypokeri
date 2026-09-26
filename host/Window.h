#ifndef POKERI_WINDOW_H
#define POKERI_WINDOW_H
#include "../src/CabinetInput.h"
#include "VideoOutput.h"
#include <string>
struct Window : pokeri::Tone {
    void *window=nullptr,*renderer=nullptr,*texture=nullptr;
    unsigned width=0,height=0;uint64_t started=0,startCycle=0;
    bool enabled=false;
    pokeri::CabinetInput cabinetInput;
    unsigned audioDevice=0,audioCount=0;
    int16_t audioBuffer[882]; // 20 ms at the AY renderer's 44.1 kHz.
    ~Window();
    static bool available();
    static std::string defaultRomDirectory();
    void open(uint64_t cycle);
    void ready(uint64_t cycle);
    void openAudio();
    void sample(int16_t value) override;
    void flushAudio();
    void finishAudio();
    bool poll(pokeri::Board &board,bool controls=true);
    void show(const pokeri::VideoFrame &frame,uint64_t cycle,unsigned cpuHz,bool paced=true);
};
#endif
