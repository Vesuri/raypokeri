#ifndef POKERI_STARTUP_TIMING_H
#define POKERI_STARTUP_TIMING_H
#include <chrono>
#include <cstdio>
#include <cstdlib>
// Optional wall-time diagnostics; never changes emulated timing.
inline void startupTiming(const char *stage) {
    if(!std::getenv("POKERI_STARTUP_TIMING"))return;
    using Clock=std::chrono::steady_clock;
    static const auto start=Clock::now();
    std::fprintf(stderr,"startup %.3f s: %s\n",std::chrono::duration<double>(Clock::now()-start).count(),stage);
}
#endif
