#ifndef POKERI_NVRAM_FILE_H
#define POKERI_NVRAM_FILE_H
#include "board/Board.h"
// Called only before takeover or after OS state is restored.
const char *loadNvram(pokeri::Nvram &nvram);
const char *saveNvram(const pokeri::Nvram &nvram);
const char *loadAccounting(uint8_t *memory,bool &loaded);
const char *saveAccounting(const uint8_t *memory);
#endif
