# TIME_LEDGER=1 LEDGER_FAST_CACHE=1, native-benchmark marker; read-only.
# Sixteen complete cards: four per mode (none/raster/controls/absolute).
set pagination off
break nativeReturned
continue
printf "RASTER baseline=%u raster=%u controls=%u absolute=%u hits=%u\n",nativeRasterBenchTicks[0],nativeRasterBenchTicks[1],nativeRasterBenchTicks[2],nativeRasterBenchTicks[3],nativeRasterHits
printf "LEDGER readcost=%u fastcache=%u\n",NativeTiming::ledgerReadCost,NativeTiming::fastCache
printf "CARD COST count=%u dropped=%u\n",NativeTiming::cardCostCount,NativeTiming::cardCostDropped
printf "EVENTS count=%u dropped=%u\n",NativeTiming::eventCount,NativeTiming::eventDropped
p nativeStatus
p nativeError
p videoDevice->commandLog
p pendingFrames
p seenFrames
p screen.frames
dump binary memory ../tmp/raster-ledger-card-cost.bin NativeTiming::cardCosts NativeTiming::cardCosts+NativeTiming::cardCostCount
dump binary memory ../tmp/raster-ledger-events.bin NativeTiming::events NativeTiming::events+NativeTiming::eventCount
detach
quit
