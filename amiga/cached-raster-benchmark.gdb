set pagination off
break nativeReturned
continue
printf "RASTER ring-before=%u ring-after=%u hits=%u\n",nativeRasterBenchTicks[0],nativeRasterBenchTicks[1],nativeRasterHits
printf "CACHE flags=%x\n",nativeBenchCacheBits
p nativeStatus
p nativeError
p nativeVectorsRestored
p NativeTiming::frequency
detach
quit
