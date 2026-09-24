#include "VideoOutput.h"
#include <cstdio>
#include <stdexcept>
// Deliberately labelled diagnostic palette; not a claim about resistor wiring.
static std::array<unsigned,16> rgb={0x000000,0x0000aa,0x00aa00,0x00aaaa,0xaa0000,0xaa00aa,0xaa5500,0xaaaaaa,0x555555,0x5555ff,0x55ff55,0x55ffff,0xff5555,0xff55ff,0xffff55,0xffffff};

static const char *paletteLabel="Placeholder palette";
unsigned frameColor(unsigned index){return rgb[index&15];}
void setFramePalette(const std::array<unsigned,16> &colors){rgb=colors;paletteLabel="ROM RAMDAC candidate (512 KB hardware unverified)";}
void writeFrame(const std::string &path,const VideoFrame &f) {
    if(!f.width || !f.height) return;
    FILE *file=fopen(path.c_str(),"wb");if(!file)throw std::runtime_error("cannot write frame");
    fprintf(file,"P6\n# %s; nominal programmed window width\n%u %u\n255\n",paletteLabel,f.width,f.height);
    for(auto i:f.indices){unsigned color=frameColor(i);unsigned char p[]={uint8_t(color>>16),uint8_t(color>>8),uint8_t(color)};fwrite(p,1,3,file);}
    if(fclose(file))throw std::runtime_error("frame write failed");
}
