# Read-only; offsets verified for frozen 33e60d3. Revalidate before reuse.
# See docs/native-video-irq-fast-path.md. No target calls or writes.
set pagination off
break nativePlayReady
continue
printf "READY frame=%u cycles=%u sampler=%u\n",pendingFrames,nativeCycles,NativeTiming::active
disable 1
set $card = (unsigned)&_ZN6pokeri13CardBackCache7commandERNS_7Hd63484EPKtj
break *($card+0x2a0)
commands
silent
if nativeCardCache->starts >= 24 && nativeCardCache->starts <= 27
printf "CARD begin frame=%u cycles=%u starts=%u hits=%u beam=%u\n",pendingFrames,nativeCycles,nativeCardCache->starts,nativeCardCache->hits,(*(unsigned long*)0xdff004 & 0x1ff00)>>8
enable 4
end
continue
end
break *($card+0x21a)
commands
silent
if nativeCardCache->starts >= 24 && nativeCardCache->starts <= 27
printf "CARD hit frame=%u cycles=%u starts=%u hits=%u beam=%u\n",pendingFrames,nativeCycles,nativeCardCache->starts,nativeCardCache->hits,(*(unsigned long*)0xdff004 & 0x1ff00)>>8
disable 4
end
continue
end
break *((unsigned)&nativeDispatch+0xc0)
commands
silent
if nativeRegisters.pc-romBase == 0x2ebc
printf "IRQGUARD card=%u frame=%u seen=%u ticks=%u phase=%u sr=%x sp=%x ar=%u mask=%u status=%u liveirq=%u credit=%u debt=%u clockframe=%u refresh=%u pending=%d tickframe=%x shuffle=%u held=%u fault=%u reset=%u quit=%u drained=%u\n",nativeCardCache->starts,pendingFrames,seenFrames,liveTicks,guestClockPhase,nativeRegisters.sr,nativeRegisters.a[7],board->video.ar,board->video.control.values[3],nativeCachedVideoStatus,liveIrqActive,liveClock.credit,liveClock.debt,liveClock.frame,compositionPending,screen.pending,presentationTickFrame,shuffleQueue.count,shuffleQueue.held,board->fault,board->resetRequested,quitRequested,nativeShortDrained
printf "SOURCES pia=%x/%x/%x/%x serial=%x/%u videoerror=%x clock=%u/%u overhead=%u ram=%x/%x\n",board->pia[0].control[0],board->pia[0].flags[0],board->pia[0].control[1],board->pia[0].flags[1],board->serial[0].control,board->serial[0].receive.n,board->video.error,nativeClockMode,clockDisplayCalibrated,nativeClockOverhead,nativeRamBegin,nativeRamEnd
end
continue
end
disable 4
break nativeReturned
continue
printf "END status=%u error=%x resets=%u frames=%u cycles=%u inputs=%u shuffle=%u audio=%u sampler=%u\n",nativeStatus,nativeError,nativeLiveWatchdogResets,pendingFrames,nativeCycles,testInputIndex,nativeShuffleSteps,nativeShuffleAyWrites,NativeTiming::active
detach
quit
