# FS-UAE instruction trace (TRACE_CODE=1 build): consecutive 100-field
# captures from the first original instruction to Ready. Use trace.sh.
set pagination off
tbreak nativeClockCalibrateBegin
continue
printf "TRACE base rom=%x\n", romBase
printf "TRACE vectors l1=%x l5=%x l7=%x\n", nativeVectors[25], nativeVectors[29], nativeVectors[31]
set $i = 0
while nativeSetupReady == 0 && nativeStatus <= 1 && $i < 40
  printf "TRACE start %d cycles=%u frames=%u stage=%d\n", $i, nativeCycles, pendingFrames, startup.stage
  eval "monitor profile 100 \"\" \"@OUT@/trace-startup-%03d.bin\"", $i
  set $i = $i + 1
end
printf "TRACE done %d cycles=%u frames=%u ready=%u status=%u stage=%d\n", $i, nativeCycles, pendingFrames, nativeSetupReady, nativeStatus, startup.stage
detach
quit
