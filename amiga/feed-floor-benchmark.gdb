set pagination off
break nativePrepareAbort
break nativeReturned
continue
printf "FLOOR rate=%u words=%u status=%u error=%x frames=%u cycles=%u bypass=%u\n",NativeTiming::frequency,512,nativeStatus,nativeError,pendingFrames,nativeCycles,nativeFeedFloorBypass
p nativeFeedFloorTicks
p nativeError
detach
quit
