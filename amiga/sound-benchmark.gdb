# Paired synthetic whole AY-register writes; four alternating-order batches.
set pagination off
break nativePrepareAbort
break nativeReturned
continue
p nativeStatus
p nativeError
p nativeVectorsRestored
p NativeTiming::frequency
p nativeSoundBenchTicks
detach
quit
