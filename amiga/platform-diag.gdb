# Read-only captures: run with the full boot replay and native-live marker.
break nativeBootReady
break nativeReturned
while nativeBootVerified == 0 && (nativeStatus == 0 || nativeStatus == 1)
continue
printf "progress status=%u boot=%u count=%u cycles=%u irqs=%u pc=%x\n",nativeStatus,nativeBootVerified,nativeInstructions,nativeCycles,nativeInterrupts,nativeLastPc
end
if nativeBootVerified == 1
printf "boot count=%u cycles=%u irqs=%u pc=%x\n",nativeInstructions,nativeCycles,nativeInterrupts,nativeLastPc
printf "bases rom=%x ram=%x guard=%x\n",romBase,ramBase,guardBase
printf "AY writes=%u hash=%u\n",paula.writeCount,paula.streamHash
dump binary memory ../tmp/native-platform-boot-ram.bin ramBase ramBase+0x40000
dump binary memory ../tmp/native-platform-boot-vram.bin videoSurface.data videoSurface.data+0x40000
set $screen_index = screen.pending >= 0 ? screen.pending : screen.front
dump binary memory ../tmp/native-platform-boot-screen.bin screen.buffers[$screen_index] screen.buffers[$screen_index]+40752
end
disable 1
while nativeStatus == 0 || nativeStatus == 1 || nativeVectorsRestored == 0
continue
printf "progress status=%u boot=%u count=%u cycles=%u irqs=%u pc=%x\n",nativeStatus,nativeBootVerified,nativeInstructions,nativeCycles,nativeInterrupts,nativeLastPc
end
printf "native status=%u boot=%u count=%u cycles=%u irqs=%u pc=%x\n",nativeStatus,nativeBootVerified,nativeInstructions,nativeCycles,nativeInterrupts,nativeLastPc
printf "vectors restored=%u\n",nativeVectorsRestored
printf "native error=%p\n",nativeError
p nativeError
printf "live watchdog resets=%u first PC=%x first elapsed cycles=%u\n",nativeLiveWatchdogResets,nativeFirstResetPc,nativeFirstResetCycle
p nativeRegisters
p liveTicks
p board->watchdogAge
p videoSurface.fills
p videoSurface.copies
p screen.frames
p videoSurface.tested
p testInputIndex
p liveCycles
p board->systemEdges
p board->video.commands
p board->pia[1].input
p nativeBlitInterrupts
p AmigaHardware::blitterSubmitted
p AmigaHardware::blitterQueued
p AmigaHardware::blitterBackpressure
p NativeTiming::frequency
p NativeTiming::readOverhead
p NativeTiming::elapsed
p NativeTiming::records
set $screen_index = screen.pending >= 0 ? screen.pending : screen.front
dump binary memory ../tmp/native-platform-live-screen.bin screen.buffers[$screen_index] screen.buffers[$screen_index]+40752
detach
quit
