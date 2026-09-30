# Synthetic setup paths: each row is ordinary/fused, 512 repetitions.
set pagination off
break nativePrepareAbort
break nativeReturned
continue
p nativeStatus
p nativeError
p nativeVectorsRestored
p NativeTiming::frequency
p nativeSetupBenchTicks
detach
quit
