set pagination off
break nativeReturned
continue
printf "RASTER baseline=%u raster=%u controls=%u absolute=%u hits=%u\n",nativeRasterBenchTicks[0],nativeRasterBenchTicks[1],nativeRasterBenchTicks[2],nativeRasterBenchTicks[3],nativeRasterHits
printf "CACHE flags=%x\n",nativeBenchCacheBits
printf "SCHED benchmark=%u pending=%u seen=%u display=%u\n",nativeBenchmarkRequested,pendingFrames,seenFrames,screen.frames
p nativeStatus
p nativeError
p nativeVectorsRestored
p NativeTiming::frequency
detach
quit
