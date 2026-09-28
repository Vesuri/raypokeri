#include "../src/platform/amiga/NativeTiming.h"
#include <cassert>
#include <cstdio>
namespace NativeTiming {
bool active=true;unsigned context=Count,commandGroup=64;uint32_t calls[Count]={},kindTicks[Count]={},clockValue=0;
Scope *Scope::top=nullptr;Ledger ledger;SlowCommand *slowCommands=nullptr;volatile uint32_t slowCount=0;
uint16_t commandWords[8];uint32_t ledgerNow(){return clockValue;}uint32_t slowCycles(){return 0;}
}
int main(){using namespace NativeTiming;uint32_t out[Count];
clockValue=10;{
 Scope service(Service);clockValue=20;{
 Scope command(Command);clockValue=30;{
 Scope blit(BlitWait);clockValue=40;Scope::snapshot(out,clockValue);
 assert(out[Service]==30 && out[Command]==20 && out[BlitWait]==10);clockValue=50;
 }clockValue=55;Scope::snapshot(out,clockValue);
 assert(out[Service]==45 && out[Command]==35 && out[BlitWait]==20);clockValue=60;
 }clockValue=70;
}Scope::snapshot(out,85);assert(out[Service]==60 && out[Command]==40 && out[BlitWait]==20);
assert(context==Count);
clockValue=100;{Scope outer(Service);clockValue=110;{Scope same(Service);clockValue=120;
 Scope audio(AyVbi);Scope disabled(Command,0,false);Scope::snapshot(out,clockValue);
 assert(out[Service]==80 && out[Command]==40 && out[AyVbi]==0);clockValue=130;
 }clockValue=140;}Scope::snapshot(out,150);assert(out[Service]==100 && context==Count);
for(unsigned k=0;k<Count;++k)kindTicks[k]=0;
clockValue=0xfffffff0u;{Scope wrap(Service);clockValue=16;Scope::snapshot(out,clockValue);assert(out[Service]==32);clockValue=32;}
assert(kindTicks[Service]==48 && context==Count);
puts("PASS: open/nested/completed scope accounting, untimed audio, disabled scopes and clock wrap");
}
