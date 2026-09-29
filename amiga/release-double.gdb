# Read-only DOUBLE_SCENARIO=1 normal-code build: release-timing.gdb plus the
# scenario result. PAL beam/frame time, not host stopwatch time.
set pagination off
break nativePlayReady
break nativeReturned
continue
if nativeSetupReady == 0 || nativeError != 0
 echo Game did not reach Ready.\n
 quit 1
end
printf "RELEASE ready cycle=%u frame=%u beam=%u\n",nativeCycles,pendingFrames,(*(unsigned long*)0xdff004>>8)&511
# diag_run.sh resolves these markers using this executable's DWARF and ABI.
# Verify the full instruction again in loaded memory before setting probes.
set $card=(char*)&_ZN6pokeri13CardBackCache7commandERNS_7Hd63484EPKtj
if *(unsigned long*)($card+@CARD_BEGIN_OFFSET@) != @CARD_BEGIN_INSTRUCTION@ || *(unsigned long*)($card+@CARD_HIT_OFFSET@) != @CARD_HIT_INSTRUCTION@
 echo Card counter instructions changed; revalidate probe.\n
 quit 1
end
break *(&amigaInputKey)
commands
silent
printf "RELEASE key cycle=%u frame=%u beam=%u index=%u code=%u down=%u double_ready=%u\n",nativeCycles,pendingFrames,(*(unsigned long*)0xdff004>>8)&511,testInputIndex,*(unsigned long*)($sp+4),*(unsigned long*)($sp+8),board->memory.values[0x4112f]
continue
end
break *(&_ZN7PaulaAy5writeEjh)
commands
silent
printf "RELEASE ay cycle=%u frame=%u beam=%u reg=%u value=%u\n",nativeCycles,pendingFrames,(*(unsigned long*)0xdff004>>8)&511,*(unsigned long*)($sp+8),*(unsigned long*)($sp+12)&255
continue
end
break *($card+@CARD_BEGIN_OFFSET@)
commands
silent
printf "RELEASE card_begin cycle=%u frame=%u beam=%u card=%u\n",nativeCycles,pendingFrames,(*(unsigned long*)0xdff004>>8)&511,nativeCardCache->starts+1
continue
end
break *($card+@CARD_HIT_OFFSET@)
commands
silent
printf "RELEASE card_hit cycle=%u frame=%u beam=%u card=%u\n",nativeCycles,pendingFrames,(*(unsigned long*)0xdff004>>8)&511,nativeCardCache->starts
continue
end
break *(romBase+0x1818a)
commands
silent
printf "RELEASE double_accepted cycle=%u frame=%u beam=%u \n",nativeCycles,pendingFrames,(*(unsigned long*)0xdff004>>8)&511
continue
end
continue
printf "RELEASE end cycle=%u frame=%u status=%u inputs=%u resets=%u error=%x vectors=%u swaps=%u screen_frames=%u\n",nativeCycles,pendingFrames,nativeStatus,testInputIndex,nativeLiveWatchdogResets,nativeError,nativeVectorsRestored,screen.swaps,screen.frames
printf "DOUBLE rounds=%u offered=%u done=%u failed=%u\n",nativeDoubleScenario.round,nativeDoubleScenario.doubled,nativeDoubleScenario.done,nativeDoubleScenario.failed
p nativeError
detach
quit
