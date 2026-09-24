#include "Pokeri.h"

int main(int argc, char** argv)
{
    Pokeri pokeri;
    if (pokeri.isRunnable()) {
        pokeri.run();
    }
    return 0;
}
