#ifndef POKERI_HOST_OPCODE_AUDIT_H
#define POKERI_HOST_OPCODE_AUDIT_H
#include <cstdint>
#include <unordered_map>
// Host-only observations at instruction entry. Keep the word seen then:
// a final RAM dump cannot describe self-modifying or overwritten RAM code.
struct OpcodeAudit {
    std::unordered_map<uint64_t,uint64_t> entries;
    void observe(uint32_t pc,uint16_t word){++entries[(uint64_t(pc)<<16)|word];}
    // MC68060UM C.2: MOVEP is the only unimplemented integer opcode family
    // available to a 68000. Reject later-ISA words separately in the report.
    static bool movep(uint16_t word){return (word&0xf138)==0x0108;}
};
#endif
