# RASTER_CHUNKS=1 and an isolated native-benchmark drive; read-only.
set pagination off
break nativePrepareAbort
break nativeReturned
continue
printf "CHUNKS rate=%u status=%u error=%x frames=%u cycles=%u\n",NativeTiming::frequency,nativeStatus,nativeError,pendingFrames,nativeCycles
printf "rows back/white; columns whole/10-word/1-word; four cards each\n"
p nativeChunkRasterTicks
p nativeError
detach
quit
