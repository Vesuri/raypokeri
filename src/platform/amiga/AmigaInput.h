#ifndef POKERI_AMIGA_INPUT_H
#define POKERI_AMIGA_INPUT_H
#include "board/Board.h"
bool amigaInputStart();
void amigaInputStop();
void amigaInputApply(pokeri::Board &board);
void amigaInputObserve(uint32_t pc,const pokeri::Board &board);
bool amigaInputQuit();
bool amigaInputLamps();
// Same input path for local diagnostic scripts, never RAM/game-state writes.
void amigaInputKey(unsigned code,bool down);
#endif
