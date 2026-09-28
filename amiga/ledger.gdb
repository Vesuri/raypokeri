# Read-only TIME_LEDGER capture (make TIME_LEDGER=1, plus native-measure and
# native-test-inputs markers). Summarize with host/native_ledger.py --log.
break nativePlayReady
break nativeReturned
while nativeSetupReady == 0 && (nativeStatus == 0 || nativeStatus == 1)
continue
end
printf "READY cycles=%u frames=%u\n",nativeCycles,pendingFrames
while nativeStatus == 0 || nativeStatus == 1 || nativeVectorsRestored == 0
continue
end
printf "native status=%u cycles=%u dispatches=%u frames=%u ready=%u\n",nativeStatus,nativeCycles,nativeInstructions,pendingFrames,nativeSetupReady
printf "LEDGER readcost=%u frames=%u slow=%u\n",NativeTiming::ledgerReadCost,NativeTiming::frameCount,NativeTiming::slowCount
p nativeError
p nativeLiveWatchdogResets
p testInputIndex
p nativeShortCalls
p nativeFeedWrites
p nativeFeedLoopWords
p nativeFeedLoopTurns
p nativeFeedLoopSaved
p screen.frames
p screen.swaps
p screen.lateSwaps
p screen.arms
p screen.fullFrames
p screen.partialFrames
p videoSurface.fills
p videoSurface.copies
p videoSurface.copyRejectedBounds
p videoSurface.copyRejectedOverlap
p AmigaHardware::blitterSubmitted
p AmigaHardware::blitterQueued
p videoDevice->curveCacheHits
p videoDevice->curveCacheMisses
p liveClock
dump binary memory ../tmp/ledger-startup.bin NativeTiming::startupMarks NativeTiming::startupMarks+9
dump binary memory ../tmp/ledger-events.bin NativeTiming::events NativeTiming::events+NativeTiming::eventCount
dump binary memory ../tmp/ledger-marks.bin NativeTiming::ledgerMarks ((char*)NativeTiming::ledgerMarks)+26*sizeof(NativeTiming::Ledger)
dump binary memory ../tmp/ledger-frames.bin NativeTiming::frameRecords NativeTiming::frameRecords+NativeTiming::frameCount
dump binary memory ../tmp/ledger-slow.bin NativeTiming::slowCommands NativeTiming::slowCommands+NativeTiming::slowCount
dump binary memory ../tmp/ledger-samples.bin NativeTiming::samples NativeTiming::samples+NativeTiming::sampleCount
printf "BASE rom=%x ram=%x dispatch=%x\n",romBase,ramBase,nativeDispatch
detach
quit
