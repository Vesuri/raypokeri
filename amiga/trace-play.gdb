# FS-UAE instruction trace (TRACE_CODE=1 DOUBLE_SCENARIO=1 build): deal, draw
# and the first accepted Double of the keyboard-only scenario. Use trace.sh.
set pagination off
break nativePlayReady
continue
printf "TRACE base rom=%x\n", romBase
printf "TRACE vectors l1=%x l5=%x l7=%x\n", nativeVectors[25], nativeVectors[29], nativeVectors[31]
printf "TRACE ready cycles=%u frames=%u\n", nativeCycles, pendingFrames
delete
break *(&amigaInputKey) if *(unsigned long*)($sp+4)==0x40 && *(unsigned long*)($sp+8)==1
continue
printf "TRACE deal cycles=%u frames=%u round=%u\n", nativeCycles, pendingFrames, nativeDoubleScenario.round
monitor profile 100 "" "@OUT@/trace-deal-000.bin"
monitor profile 100 "" "@OUT@/trace-deal-001.bin"
monitor profile 100 "" "@OUT@/trace-deal-002.bin"
printf "TRACE deal-end cycles=%u frames=%u\n", nativeCycles, pendingFrames
continue
printf "TRACE draw cycles=%u frames=%u held=%u\n", nativeCycles, pendingFrames, nativeDoubleScenario.held
monitor profile 100 "" "@OUT@/trace-draw-000.bin"
monitor profile 100 "" "@OUT@/trace-draw-001.bin"
printf "TRACE draw-end cycles=%u frames=%u\n", nativeCycles, pendingFrames
delete
break *(&amigaInputKey) if *(unsigned long*)($sp+4)==0x22 && *(unsigned long*)($sp+8)==1
continue
printf "TRACE double cycles=%u frames=%u round=%u ready=%u\n", nativeCycles, pendingFrames, nativeDoubleScenario.round, board->memory.values[0x4112f]
monitor profile 100 "" "@OUT@/trace-double-000.bin"
monitor profile 100 "" "@OUT@/trace-double-001.bin"
monitor profile 100 "" "@OUT@/trace-double-002.bin"
monitor profile 100 "" "@OUT@/trace-double-003.bin"
printf "TRACE double-end cycles=%u frames=%u\n", nativeCycles, pendingFrames
delete
break nativeReturned
continue
printf "TRACE end cycles=%u frames=%u status=%u error=%x resets=%u rounds=%u doubled=%u done=%u failed=%u\n", nativeCycles, pendingFrames, nativeStatus, nativeError, nativeLiveWatchdogResets, nativeDoubleScenario.round, nativeDoubleScenario.doubled, nativeDoubleScenario.done, nativeDoubleScenario.failed
p nativeError
detach
quit
