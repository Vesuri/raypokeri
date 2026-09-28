set pagination off
break nativePrepareAbort
break nativeReturned
continue
printf "CLOCK rate=%u status=%u error=%x frames=%u cycles=%u\n",NativeTiming::frequency,nativeStatus,nativeError,pendingFrames,nativeCycles
p nativeClockBenchTicks
p nativeError
detach
quit
