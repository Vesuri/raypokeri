#include "Pokeri.h"

extern "C" void pokeriReleaseHeap();
extern "C" const char *nativeError;
extern "C" unsigned long nativeExitCode;

int main(int argc, char** argv)
{
    bool started=false;
    {
        Pokeri pokeri;
        started=pokeri.isRunnable();
        if (started) {
            pokeri.run();
        }
    }
    pokeriReleaseHeap();
    return started && !nativeError ? 0 : int(nativeExitCode);
}
