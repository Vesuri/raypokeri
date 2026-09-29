# Read-only PAL/scanline admission timing; run with a frozen matching ELF.
# From completed control write to entry of the original video handler.
set pagination off
break nativePlayReady
continue
set $active = 0
set $samples = 0
break *nativeShortControlPromote
commands
silent
set $active = 0
if *(unsigned long*)($sp+18)==nativeRomBegin+0x2ebc && nativeCardCache->starts>=25 && nativeCardCache->starts<=27
set $active = 1
set $start = pendingFrames*20000+((*(unsigned long*)0xdff004 & 0x1ff00)>>8)*64
end
continue
end
break *(*(unsigned long*)(rom+0x100))
commands
silent
if $active
set $end = pendingFrames*20000+((*(unsigned long*)0xdff004 & 0x1ff00)>>8)*64
printf "ADMISSION us=%u card=%u\n",$end-$start,nativeCardCache->starts
set $samples = $samples+1
set $active = 0
end
continue
end
break nativeReturned
continue
printf "END samples=%u status=%u error=%u resets=%u inputs=%u\n",$samples,nativeStatus,nativeError,nativeLiveWatchdogResets,testInputIndex
detach
quit
