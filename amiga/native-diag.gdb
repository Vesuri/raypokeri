break nativeReturned
while nativeStatus == 0 || nativeStatus == 1 || nativeVectorsRestored == 0
continue
printf "progress status=%u count=%u cycles=%u irqs=%u pc=%x\n", nativeStatus, nativeInstructions, nativeCycles, nativeInterrupts, nativeLastPc
end
printf "native status=%u count=%u cycles=%u irqs=%u pc=%x\n", nativeStatus, nativeInstructions, nativeCycles, nativeInterrupts, nativeLastPc
printf "vectors restored=%u\n", nativeVectorsRestored
p nativeError
p nativeRegisters
p nextEvent
printf "bases rom=%x ram=%x guard=%x\n", romBase, ramBase, guardBase
dump binary memory ../tmp/native-amiga-ram.bin ramBase ramBase+0x40000
detach
quit
