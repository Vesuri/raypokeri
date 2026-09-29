# Read-only normal executable: PAL beam/frame time, not host stopwatch time.
set pagination off
break nativePlayReady
break nativeReturned
continue
if nativeSetupReady == 0 || nativeError != 0
 echo Game did not reach Ready.\n
 quit 1
end
printf "RELEASE ready cycle=%u frame=%u beam=%u\n",nativeCycles,pendingFrames,(*(unsigned long*)0xdff004>>8)&511
# Verify counter opcodes and member offsets against this executable.
set $card=(char*)&_ZN6pokeri13CardBackCache7commandERNS_7Hd63484EPKtj
if *(unsigned long*)($card+0x2d0) != (0x52aa0000 | (unsigned)&((pokeri::CardBackCache*)0)->starts) || *(unsigned long*)($card+0x256) != (0x52aa0000 | (unsigned)&((pokeri::CardBackCache*)0)->hits)
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
break *($card+0x2d0)
commands
silent
printf "RELEASE card_begin cycle=%u frame=%u beam=%u card=%u\n",nativeCycles,pendingFrames,(*(unsigned long*)0xdff004>>8)&511,nativeCardCache->starts+1
continue
end
break *($card+0x256)
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
p nativeError
detach
quit
