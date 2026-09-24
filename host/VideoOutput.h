#ifndef POKERI_VIDEO_OUTPUT_H
#define POKERI_VIDEO_OUTPUT_H
#include "../src/board/Display.h"
#include <string>
#include <vector>
using pokeri::VideoFrame;
using pokeri::compose;
void writeFrame(const std::string &path,const VideoFrame &frame);

unsigned frameColor(unsigned index);
void setFramePalette(const std::array<unsigned,16> &colors);

#endif
