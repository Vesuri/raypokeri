#ifndef POKERI_WAV_OUTPUT_H
#define POKERI_WAV_OUTPUT_H
#include "../src/board/Board.h"
#include <cstdio>
#include <string>
struct WavOutput : pokeri::Tone {
    FILE *file=nullptr;uint32_t count=0;
    ~WavOutput();
    void open(const std::string &path);
    void sample(int16_t value) override;
    void close();
};
#endif
