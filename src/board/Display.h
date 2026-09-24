#ifndef POKERI_DISPLAY_H
#define POKERI_DISPLAY_H
#include "Hd63484.h"
namespace pokeri {
struct VideoFrame { unsigned width=0,height=0; std::vector<uint8_t> indices; };
// Cropped logical pixels. Timing frequency and RGB palette belong to the backend.
VideoFrame compose(const Hd63484 &v);
}
#endif
