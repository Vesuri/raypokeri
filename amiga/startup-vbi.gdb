# Read-only startup VBI attribution on the normal executable.
# Run a cold drive with POKERI_REPLAY=0. Stops at Ready; no target writes.
# Locate the return from screen.vbi rather than baking in a compiler offset.
set pagination off
set $scan=(char*)&'nativeVbi(bool)'
set $end=$scan+128
set $after=0
while $scan < $end && $after == 0
if *(unsigned short*)$scan == 0x4eb9
if *(unsigned long*)($scan+2) == (unsigned long)&'AmigaScreen::vbi()'
set $after=$scan+6
end
end
set $scan=$scan+2
end
if $after == 0
echo Cannot locate screen.vbi call; inspect the current disassembly.\n
quit 1
end
set $entry=0
set $entrypc=0
set $entrysr=0
set $calls=0
break *nativeLevel3
commands
silent
if (*(unsigned short*)0xdff01e & *(unsigned short*)0xdff01c & 32)
set $entry=(*(unsigned long*)0xdff004>>8)&511
set $entrypc=*(unsigned long*)($sp+2)
set $entrysr=*(unsigned short*)$sp
end
continue
end
break *'nativeVbi(bool)'
commands
silent
set $service=(*(unsigned long*)0xdff004>>8)&511
continue
end
break *$after
commands
silent
set $done=(*(unsigned long*)0xdff004>>8)&511
set $calls=$calls+1
printf "VBI %u entry=%u service=%u done=%u pc=%x sr=%x cycles=%u\n",$calls,$entry,$service,$done,$entrypc,$entrysr,nativeCycles
continue
end
break nativeClockCalibrateBegin
commands
silent
printf "CALIB begin frames=%u cycles=%u line=%u visible=%u savedpc=%x\n",pendingFrames,nativeCycles,(*(unsigned long*)0xdff004>>8)&511,screen.displaying,nativeRegisters.pc
continue
end
break nativeClockCalibrateNext if nativeClockCalibrating == 1
commands
silent
printf "CALIB last frames=%u cycles=%u line=%u speed=%u visible=%u pending=%x\n",pendingFrames,nativeCycles,(*(unsigned long*)0xdff004>>8)&511,speedCalibration,screen.displaying,*(unsigned short*)0xdff01e
continue
end
break nativePlayReady
continue
printf "READY frames=%u cycles=%u status=%u error=%x\n",pendingFrames,nativeCycles,nativeStatus,nativeError
p nativeClockMinimum
p nativeClockMaximum
p nativeClockOverhead
p nativeSpeedCycles
p cpuClockLimit
detach
quit
