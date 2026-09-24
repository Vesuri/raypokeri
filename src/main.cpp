#include "Pokeri.h"

extern "C" void pokeriReleaseHeap();

int main(int argc, char** argv)
{
    {
        Pokeri pokeri;
        if (pokeri.isRunnable()) {
            pokeri.run();
        }
    }
    pokeriReleaseHeap();
    return 0;
}
