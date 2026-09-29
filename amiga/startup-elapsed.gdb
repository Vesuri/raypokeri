# Build STARTUP_PROFILE=1; three CIA-A TOD snapshots, no hot-path timing.
# POKERI_REPLAY=0. Executable loading and early CRT precede these markers.
set pagination off
break nativePlayReady
break nativeReturned
continue
printf "STARTUP preparation=%u initialization=%u total=%u frames=%u cycles=%u retained=%u status=%u error=%x\n",(nativeStartupTicks[1]-nativeStartupTicks[0])&0xffffff,(nativeStartupTicks[2]-nativeStartupTicks[1])&0xffffff,(nativeStartupTicks[2]-nativeStartupTicks[0])&0xffffff,pendingFrames,nativeCycles,startup.retained,nativeStatus,nativeError
p nativeStartupTicks
detach
quit
