# Read-only sampler dump. Stage native-measure explicitly; normal runs stay unprofiled.
# With diag_run.sh, SIGINT ends this continue and captures the selected workload.
continue
printf "PROFILE status=%u cycles=%u dispatches=%u samples=%u dropped=%u\n",nativeStatus,nativeCycles,nativeInstructions,NativeTiming::sampleCount,NativeTiming::dropped
printf "BASE rom=%x ram=%x dispatch=%x\n",romBase,ramBase,nativeDispatch
p nativeError
p NativeTiming::calls
p NativeTiming::kinds
p NativeTiming::milestones
p NativeTiming::frequency
p NativeTiming::elapsed
p nativeClockObserved
p nativeClockCharged
p nativeClockOverhead
dump binary memory ../tmp/native-profile.bin NativeTiming::samples NativeTiming::samples+NativeTiming::sampleCount
dump binary memory ../tmp/native-profile-hooks.bin NativeTiming::hooks NativeTiming::hooks+4096
detach
quit
