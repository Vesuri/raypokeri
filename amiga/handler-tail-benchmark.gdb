# Paired 512-sequence consumer store + selector + MOVEM + RTE, display DMA on.
set pagination off
break nativePrepareAbort
break nativeReturned
continue
p nativeStatus
p nativeError
p nativeVectorsRestored
p NativeTiming::frequency
p nativeTailBenchTicks
detach
quit
