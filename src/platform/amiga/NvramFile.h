#ifndef POKERI_NVRAM_FILE_H
#define POKERI_NVRAM_FILE_H
#include "board/Board.h"
// Called only before takeover or after OS state is restored.
const char *loadNvram(pokeri::Nvram &nvram);
const char *saveNvram(const pokeri::Nvram &nvram);
#endif
