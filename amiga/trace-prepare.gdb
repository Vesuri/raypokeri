# Preparation attribution only: no target timers or profiler support.
# Captures include the full preparation and at most the remainder of its final
# 100-field block. The prepare/prepared events identify the boundary to analyze.
set pagination off
tbreak *nativePrepareInner
continue
set $i = 0
while installed == 0 && nativeStatus != 0xdead && $i < 5
  eval "monitor profile 100 \"\" \"@OUT@/trace-prepare-%03d.bin\"", $i
  set $i = $i + 1
end
if installed == 0
  echo preparation did not finish in the capture budget\n
  quit 1
end
printf "TRACE base rom=%x\n", romBase
printf "TRACE vectors l1=%x l5=%x l7=%x\n", nativeVectors[25], nativeVectors[29], nativeVectors[31]
printf "TRACE preparation captures=%u status=%u cycles=%u frames=%u\n", $i, nativeStatus, nativeCycles, pendingFrames
detach
quit
