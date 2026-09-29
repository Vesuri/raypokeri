#include "Pokeri.h"

extern "C" void pokeriReleaseHeap();
extern "C" const char *nativeError;

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
    return started && !nativeError ? 0 : 20;
}
