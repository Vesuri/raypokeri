#ifndef POKERI_AMIGA_NATIVE_H
#define POKERI_AMIGA_NATIVE_H
extern "C" bool nativePrepare();
void nativeRun();
void nativeRelease();
void nativeVbi(bool quit);
class CopperList;
CopperList *nativeCopper();
void nativeAudioStart();
void nativeAudioStop();
#endif
