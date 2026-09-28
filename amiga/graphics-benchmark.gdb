# Explicit native-benchmark only: no game assets or captures are written.
set pagination off
break nativeReturned
continue
printf "COUNTERS mode=%u\n",(unsigned)&nativeFeedCounterMode
p nativeStatus
p nativeError
p nativeVectorsRestored
p NativeTiming::frequency
p nativeRingBenchTicks
printf "INLINE before=%u after=%u words=%u\n",nativeInlineBenchTicks[0],nativeInlineBenchTicks[1],nativeFeedInlineWords
printf "HEADER before=%u after=%u words=%u\n",nativeHeaderBenchTicks[0],nativeHeaderBenchTicks[1],nativeFeedHeaderWords
printf "PATTERN expand=%u cache=%u\n",nativePatternBenchTicks[0],nativePatternBenchTicks[1]
printf "SCROLL attract=%u doubling=%u\n",nativeScrollBenchTicks[0],nativeScrollBenchTicks[1]
p nativeCardBenchTicks
p nativeDrawingBenchTicks
p nativeScreenBenchTicks
p screen.error
p screen.fullFrames
p screen.partialFrames
detach
quit
