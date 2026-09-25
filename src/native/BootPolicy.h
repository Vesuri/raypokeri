#ifndef POKERI_BOOT_POLICY_H
#define POKERI_BOOT_POLICY_H
#include <stdint.h>
namespace pokeri {
// User-approved normal-game policy: skip coin-op diagnostic work, preserving
// initialization. This is patch metadata and newly assembled instructions;
// expected original bytes are generated locally, never committed.
struct BootPatch {
    uint32_t offset,target;
    enum Kind {PcRelativeWord,Branch,ReturnZero,ReturnOne} kind;
};
static constexpr BootPatch bootPatches[]={
    {0x10ae,0x10e0,BootPatch::Branch}, // original module loader, without checksum loop
    {0x110c,0x113a,BootPatch::Branch},
    {0x121e,0x127a,BootPatch::PcRelativeWord}, // retain original RAM clear, skip pattern tests
    {0x1f2e,0,BootPatch::ReturnZero}, // PIA/AY/timer/watchdog startup tests: clear V
    {0x25a4,0,BootPatch::ReturnZero}, // known read-only program mapping, including later probes
    {0x5b9c,0,BootPatch::ReturnZero}, // fixed 512 KB display memory configuration
    {0x10f2c,0,BootPatch::ReturnOne}, // display graphics readback checksum
    {0x16e1c,0,BootPatch::ReturnOne}, // video RAM pattern and external-board tests
};
inline void applyBootPolicy(uint8_t *rom){
    auto word=[&](uint32_t at,uint16_t value){rom[at]=value>>8;rom[at+1]=value;};
    for(const auto &patch:bootPatches){
        if(patch.kind==BootPatch::PcRelativeWord)
            word(patch.offset,uint16_t(patch.target-patch.offset));
        else if(patch.kind==BootPatch::Branch){
            word(patch.offset,0x6000);word(patch.offset+2,uint16_t(patch.target-patch.offset-2));
        }else {
            word(patch.offset,patch.kind==BootPatch::ReturnOne?0x7001:0x7000); // MOVEQ #result,D0
            word(patch.offset+2,0x4e75); // RTS
        }
    }
}
}
#endif
