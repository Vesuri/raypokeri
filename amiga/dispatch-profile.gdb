# Build clean with DISPATCH_PROFILE=1. Read-only startup/gameplay counters.
# Stage native-measure and native-test-inputs.
# native-live holds the absolute big-endian cycle limit. Compute actual play
# duration from END minus READY cycles; setup is acknowledgement-driven.
break nativePlayReady
break nativeReturned
while nativeSetupReady == 0 && (nativeStatus == 0 || nativeStatus == 1)
continue
printf "progress status=%u cycles=%u ready=%u\n",nativeStatus,nativeCycles,nativeSetupReady
end
if nativeSetupReady == 0
p nativeError
quit
end
printf "READY cycles=%u dispatches=%u short=%u samples=%u\n",nativeCycles,nativeInstructions,nativeShortCalls,NativeTiming::sampleCount
dump binary memory ../tmp/dispatch-ready-hooks.bin NativeTiming::hooks NativeTiming::hooks+4096
dump binary memory ../tmp/dispatch-ready-kinds.bin NativeTiming::kinds NativeTiming::kinds+48
dump binary memory ../tmp/dispatch-ready-routines.bin NativeTiming::routines NativeTiming::routines+NativeTiming::RoutineCount
dump binary memory ../tmp/dispatch-ready-short.bin nativeShortStatus nativeShortStatus+nativeShortCount
dump binary memory ../tmp/dispatch-ready-traps.bin nativeShortTraps nativeShortTraps+16
disable 1
while nativeStatus == 0 || nativeStatus == 1 || nativeVectorsRestored == 0
continue
printf "progress status=%u cycles=%u ready=%u\n",nativeStatus,nativeCycles,nativeSetupReady
end
printf "END status=%u cycles=%u dispatches=%u short=%u samples=%u\n",nativeStatus,nativeCycles,nativeInstructions,nativeShortCalls,NativeTiming::sampleCount
printf "BASE rom=%x ram=%x dispatch=%x\n",romBase,ramBase,nativeDispatch
p nativeError
p nativeVectorsRestored
p nativeLiveWatchdogResets
p testInputIndex
p liveClock
p NativeTiming::elapsed
p NativeTiming::frequency
p NativeTiming::milestones
p NativeTiming::calls
dump binary memory ../tmp/dispatch-end-hooks.bin NativeTiming::hooks NativeTiming::hooks+4096
dump binary memory ../tmp/dispatch-end-kinds.bin NativeTiming::kinds NativeTiming::kinds+48
dump binary memory ../tmp/dispatch-end-routines.bin NativeTiming::routines NativeTiming::routines+NativeTiming::RoutineCount
dump binary memory ../tmp/dispatch-end-short.bin nativeShortStatus nativeShortStatus+nativeShortCount
dump binary memory ../tmp/dispatch-end-traps.bin nativeShortTraps nativeShortTraps+16
dump binary memory ../tmp/dispatch-samples.bin NativeTiming::samples NativeTiming::samples+NativeTiming::sampleCount
detach
quit
