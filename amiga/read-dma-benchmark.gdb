# Build READ_ONLY_DMA=1 and launch with native-benchmark in the drive.
# Synthetic display/guard probes only; no game state is injected.
set pagination off
break nativeReturned
continue
printf "READDMA probes-serial=%u probes-overlap=%u total-serial=%u total-overlap=%u\n",nativeReadDmaTicks[0],nativeReadDmaTicks[1],nativeReadDmaTotal[0],nativeReadDmaTotal[1]
p nativeStatus
p nativeError
p nativeVectorsRestored
p NativeTiming::frequency
detach
quit
