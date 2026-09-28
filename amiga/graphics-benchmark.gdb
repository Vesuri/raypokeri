# Explicit native-benchmark only: no game assets or captures are written.
set pagination off
break nativeReturned
continue
p nativeStatus
p nativeError
p nativeVectorsRestored
p NativeTiming::frequency
p nativeRingBenchTicks
printf "INLINE before=%u after=%u words=%u\n",nativeInlineBenchTicks[0],nativeInlineBenchTicks[1],nativeFeedInlineWords
printf "PATTERN expand=%u cache=%u\n",nativePatternBenchTicks[0],nativePatternBenchTicks[1]
p nativeCardBenchTicks
p nativeDrawingBenchTicks
p nativeScreenBenchTicks
p screen.error
p screen.fullFrames
p screen.partialFrames
detach
quit
