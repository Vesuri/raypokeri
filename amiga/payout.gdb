# PAYOUT_SCENARIO=1, native-test-inputs, sufficiently large native-live budget.
# Read-only; only the external keyboard driver supplies game inputs.
set pagination off
break nativeReturned
continue
printf "PAYOUT status=%u error=%x resets=%u vectors=%u inputs=%u\n",nativeStatus,nativeError,nativeLiveWatchdogResets,nativeVectorsRestored,testInputIndex
p nativePayoutScenario
p nativeError
if !nativePayoutScenario.done || nativePayoutScenario.failed || nativeError || nativeLiveWatchdogResets || !nativeVectorsRestored
 detach
 quit 1
end
echo PASS: win, Collect, payout, coin and next deal.\n
detach
quit
