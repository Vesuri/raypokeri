# RASTER_SAMPLES=1; read-only capture after 512 complete cached backs.
set pagination off
break nativeReturned
continue
printf "BASE rom=%x ram=%x dispatch=%x\n",romBase,ramBase,nativeDispatch
printf "SAMPLES count=%u dropped=%u\n",NativeTiming::sampleCount,NativeTiming::dropped
printf "RASTER ticks=%u hits=%u\n",nativeRasterBenchTicks[3],nativeRasterHits
p nativeStatus
p nativeError
p pendingFrames
p seenFrames
p screen.frames
dump binary memory ../tmp/cached-raster-samples.bin NativeTiming::samples NativeTiming::samples+NativeTiming::sampleCount
detach
quit
