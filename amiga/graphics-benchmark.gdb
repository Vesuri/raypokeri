# Explicit native-benchmark only: no game assets or captures are written.
set pagination off
break nativeReturned
continue
p nativeStatus
p nativeError
p nativeVectorsRestored
p NativeTiming::frequency
p nativeRingBenchTicks
p nativeCardBenchTicks
p nativeDrawingBenchTicks
p nativeScreenBenchTicks
p screen.error
p screen.fullFrames
p screen.partialFrames
detach
quit
